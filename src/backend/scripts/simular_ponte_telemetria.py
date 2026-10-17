"""Simula a ponte (POST /telemetria) com ritmo ~1 Hz e telemetria plausível.

Envia health-check, depois `tel` a cada intervalo (bateria caindo) na célula
do odômetro (`passo`), opcionalmente avança o trajeto largada→meta (4x4).
Com `--encerrar sucesso`, emite os passos que faltarem até (3,3) antes do
evento. Após cada mensagem aceita, consulta GET /execucoes/{id} e imprime
consumo de bateria (mV e %), tempo e velocidade média da tentativa aberta.

Requer API no ar (uvicorn) e migrações aplicadas.

Uso, a partir de src/backend:
    python -m scripts.simular_ponte_telemetria
    python -m scripts.simular_ponte_telemetria --ticks 20 --queda-mv 20 --passo-a-cada 2
    python -m scripts.simular_ponte_telemetria --encerrar sucesso
    python -m scripts.simular_ponte_telemetria --encerrar cancelada
    python -m scripts.simular_ponte_telemetria --encerrar interrompida
    python -m scripts.simular_ponte_telemetria --health-check item-falha
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import UTC, datetime

import httpx

# (x, y, paredes_mask) — passos encadeados da largada à meta (4.4.1 / canto 4x4).
# tel usa a mesma célula do último passo (odometria); sucesso só na meta após percorrer.
_TRAJETOS: dict[str, list[tuple[int, int, int]]] = {
    "4x4": [
        (0, 0, 10),  # O+S — largada
        (0, 1, 8),  # sobe pela coluna oeste
        (0, 2, 8),
        (0, 3, 9),  # N+O (exemplo do contrato)
        (1, 3, 1),  # corredor superior leste
        (2, 3, 1),
        (3, 3, 5),  # N+L — meta (3,3)
    ],
}


def _trajeto(labirinto: str) -> list[tuple[int, int, int]]:
    return _TRAJETOS.get(labirinto, _TRAJETOS["4x4"])


def _meta(labirinto: str) -> tuple[int, int]:
    ultima = _trajeto(labirinto)[-1]
    return ultima[0], ultima[1]


# ADR-009 — ordem típica dos hc_item no firmware
_COMPONENTES_HC: tuple[str, ...] = (
    "bateria",
    "tof_frontal_esq",
    "tof_frontal_dir",
    "tof_esquerdo",
    "tof_direito",
    "motor_esquerdo",
    "motor_direito",
    "encoder_esquerdo",
    "encoder_direito",
)


def _iso_agora() -> str:
    return datetime.now(UTC).isoformat().replace("+00:00", "Z")


def _linha(obj: dict) -> str:
    return json.dumps(obj, separators=(",", ":"))


def _post_telemetria(client: httpx.Client, linha: str) -> httpx.Response:
    return client.post(
        "/telemetria",
        json={"linha": linha, "recebido_em": _iso_agora()},
    )


def _hc_item(
    boot: int,
    seq: int,
    t_ms: int,
    *,
    componente: str,
    aprovado: bool,
    valor: int | None,
) -> str:
    return _linha(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "hc_item",
            "componente": componente,
            "aprovado": aprovado,
            "valor": valor,
        }
    )


def _hc_resultado(
    boot: int,
    seq: int,
    t_ms: int,
    *,
    tipo_dip: str,
    aprovado: bool,
    inicio: str = "nova",
) -> str:
    return _linha(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "hc_resultado",
            "aprovado": aprovado,
            "tipo_dip": tipo_dip,
            "inicio": inicio,
        }
    )


def _tel(
    boot: int,
    seq: int,
    t_ms: int,
    *,
    x: int,
    y: int,
    bat_mv: int,
    vel_mm_s: int,
    estado: str = "running",
) -> str:
    return _linha(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "tel",
            "estado": estado,
            "x": x,
            "y": y,
            "rumo": "N",
            "bat_mv": bat_mv,
            "vel_mm_s": vel_mm_s,
            "eixo_longo": None,
        }
    )


def _passo(boot: int, seq: int, t_ms: int, x: int, y: int, paredes: int) -> str:
    return _linha(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "passo",
            "x": x,
            "y": y,
            "paredes": paredes,
        }
    )


def _falha_web(boot: int, seq: int, t_ms: int, x: int, y: int) -> str:
    return _linha(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "falha",
            "motivo": None,
            "origem": "web",
            "x": x,
            "y": y,
            "componente": None,
        }
    )


def _sucesso(boot: int, seq: int, t_ms: int, x: int, y: int) -> str:
    return _linha(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "sucesso",
            "x": x,
            "y": y,
        }
    )


def _ultima_tentativa(detalhe: dict) -> dict | None:
    tentativas = detalhe.get("tentativas") or []
    if not tentativas:
        return None
    return max(tentativas, key=lambda t: t["attempt_index"])


def _fmt(valor) -> str:
    if valor is None:
        return "—"
    if isinstance(valor, float):
        return f"{valor:.4g}"
    return str(valor)


def _imprimir_cabecalho() -> None:
    print(
        "tick  seq  bat_mv  cons_mV  cons_%   tempo_s  vel_med  status      msg"
    )
    print("-" * 78)


def _imprimir_linha(
    tick: int,
    seq: int,
    bat_mv: int | None,
    tentativa: dict | None,
    msg: str,
) -> None:
    t = tentativa or {}
    print(
        f"{tick:4d}  {seq:3d}  "
        f"{bat_mv if bat_mv is not None else '—':>6}  "
        f"{_fmt(t.get('consumo_bateria')):>7}  "
        f"{_fmt(t.get('consumo_bateria_pct')):>6}  "
        f"{_fmt(t.get('tempo_s')):>7}  "
        f"{_fmt(t.get('velocidade_media')):>7}  "
        f"{t.get('status', '—'):<11}  "
        f"{msg}"
    )


def _celula_atual(trajeto: list[tuple[int, int, int]], indice: int) -> tuple[int, int]:
    x, y, _ = trajeto[indice]
    return x, y


def _enviar_passo_trajeto(
    client: httpx.Client,
    execucao_id: str,
    *,
    boot: int,
    seq: int,
    t_ms: int,
    trajeto: list[tuple[int, int, int]],
    indice: int,
    tick: int,
    rotulo_extra: str = "",
) -> tuple[int, int, httpx.Response]:
    x, y, paredes = trajeto[indice]
    resp = _post_telemetria(client, _passo(boot, seq, t_ms, x, y, paredes))
    if resp.status_code == 200:
        detalhe = client.get(f"/execucoes/{execucao_id}").json()
        msg = f"passo ({x},{y}){rotulo_extra}"
        _imprimir_linha(tick, seq, None, _ultima_tentativa(detalhe), msg)
    return x, y, resp


def _completar_trajeto_ate_meta(
    client: httpx.Client,
    execucao_id: str,
    args: argparse.Namespace,
    *,
    boot: int,
    seq: int,
    t_ms: int,
    trajeto: list[tuple[int, int, int]],
    indice: int,
    tick: int,
) -> tuple[int, int, int]:
    """Emite passos restantes até a meta (índice len-1)."""
    while indice < len(trajeto) - 1:
        time.sleep(args.intervalo)
        indice += 1
        seq += 1
        t_ms += int(args.intervalo * 1000)
        _, _, resp = _enviar_passo_trajeto(
            client,
            execucao_id,
            boot=boot,
            seq=seq,
            t_ms=t_ms,
            trajeto=trajeto,
            indice=indice,
            tick=tick,
            rotulo_extra=" → meta",
        )
        if resp.status_code != 200:
            print(f"passo meta: {resp.status_code} {resp.text}", file=sys.stderr)
            raise RuntimeError("passo rejeitado ao completar trajeto")
    return seq, t_ms, indice


def _simular_health_check(
    client: httpx.Client,
    execucao_id: str,
    args: argparse.Namespace,
    *,
    boot: int,
    seq: int,
    t_ms: int,
) -> tuple[int, int, bool]:
    """Envia HC; retorna (seq, t_ms, continuar_corrida). continuar=False se tentativa failed."""
    modo = args.health_check
    falha_em = args.componente_falha

    def _enviar(linha: str, rotulo: str) -> bool:
        nonlocal seq, t_ms
        resp = _post_telemetria(client, linha)
        if resp.status_code != 200:
            print(f"{rotulo}: {resp.status_code} {resp.text}", file=sys.stderr)
            return False
        detalhe = client.get(f"/execucoes/{execucao_id}").json()
        _imprimir_linha(-1, seq, None, _ultima_tentativa(detalhe), rotulo)
        return True

    if modo in ("aprovado", "item-falha"):
        for componente in _COMPONENTES_HC:
            seq += 1
            t_ms += 80
            aprovado = True
            if modo == "item-falha" and componente == falha_em:
                aprovado = False
            valor = 7840 if componente == "bateria" and aprovado else None
            if not _enviar(
                _hc_item(
                    boot,
                    seq,
                    t_ms,
                    componente=componente,
                    aprovado=aprovado,
                    valor=valor,
                ),
                f"hc_item {componente}={'ok' if aprovado else 'FALHA'}",
            ):
                return seq, t_ms, False
            if args.intervalo > 0:
                time.sleep(min(args.intervalo, 0.3))

    if modo == "dip-errado":
        dip = {"4x4": "8x4", "8x4": "4x4", "12x4": "4x4"}[args.labirinto]
        hc_aprovado = True
    elif modo in ("resultado-falha", "item-falha"):
        dip = args.labirinto
        hc_aprovado = False
    else:
        dip = args.labirinto
        hc_aprovado = True

    seq += 1
    t_ms += 100
    if not _enviar(
        _hc_resultado(
            boot,
            seq,
            t_ms,
            tipo_dip=dip,
            aprovado=hc_aprovado,
        ),
        f"hc_resultado aprovado={hc_aprovado} dip={dip}",
    ):
        return seq, t_ms, False

    detalhe = client.get(f"/execucoes/{execucao_id}").json()
    tent = _ultima_tentativa(detalhe)
    continuar = tent is not None and tent.get("status") == "running"
    if not continuar:
        falha = (tent or {}).get("falha") or {}
        print(
            f"health-check encerrou tentativa: status={tent.get('status') if tent else '?'} "
            f"motivo={falha.get('motivo')} execucao={detalhe.get('status')}"
        )
    return seq, t_ms, continuar


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", default="http://localhost:8000", help="URL base da API")
    parser.add_argument(
        "--labirinto",
        default="4x4",
        choices=("4x4", "8x4", "12x4"),
        help="tipo do labirinto (deve bater com tipo_dip do HC)",
    )
    parser.add_argument("--intervalo", type=float, default=1.0, help="segundos entre envios")
    parser.add_argument("--ticks", type=int, default=15, help="quantidade de ciclos (tel)")
    parser.add_argument("--bat-inicial", type=int, default=8400, help="bat_mv da primeira tel")
    parser.add_argument(
        "--queda-mv",
        type=int,
        default=12,
        help="queda de bat_mv por tick (simula descarga)",
    )
    parser.add_argument("--vel-mm-s", type=int, default=210, help="vel_mm_s nas tel")
    parser.add_argument(
        "--passo-a-cada",
        type=int,
        default=3,
        help="a cada N ticks envia passo (0 desliga); usa células do trajeto",
    )
    parser.add_argument(
        "--encerrar",
        choices=("nenhum", "sucesso", "falha", "cancelada", "interrompida"),
        default="nenhum",
        help=(
            "fim simulado: sucesso/falha (collision); cancelada = link_lost (RF32); "
            "interrompida = operador encerra tentativa (execução em_andamento, cmd interromper)"
        ),
    )
    parser.add_argument(
        "--motivo-encerramento",
        default="collision",
        choices=("collision", "stuck", "out_of_track"),
        help="motivo em POST /encerrar-tentativa (modo interrompida)",
    )
    parser.add_argument(
        "--confirmar-firmware",
        action="store_true",
        help="após interrompida, envia falha origem web (confirmação serial)",
    )
    parser.add_argument(
        "--espera-link-perdido",
        type=float,
        default=11.0,
        help="segundos sem POST antes da tel de cancelada (API: LIMITE_SEM_SINAL_S, padrão 10)",
    )
    parser.add_argument(
        "--health-check",
        default="aprovado",
        choices=("aprovado", "item-falha", "resultado-falha", "dip-errado"),
        help=(
            "aprovado: 9 hc_item + hc_resultado ok; item-falha: um componente reprova; "
            "resultado-falha: só hc_resultado reprovado; dip-errado: tipo_dip ≠ labirinto"
        ),
    )
    parser.add_argument(
        "--componente-falha",
        default="motor_esquerdo",
        choices=_COMPONENTES_HC,
        help="componente reprovado em --health-check item-falha",
    )
    args = parser.parse_args()

    boot = int(time.time()) % (2**32 - 1)
    seq = 0
    t_ms = 500
    timeout_http = max(10.0, args.espera_link_perdido + 5.0)

    with httpx.Client(base_url=args.url, timeout=timeout_http) as client:
        try:
            health = client.get("/back-health")
            health.raise_for_status()
        except httpx.HTTPError as erro:
            print(f"API indisponível em {args.url}: {erro}", file=sys.stderr)
            return 1

        criado = client.post("/execucoes", json={"tipo_labirinto": args.labirinto})
        if criado.status_code != 201:
            print(f"POST /execucoes: {criado.status_code} {criado.text}", file=sys.stderr)
            return 1
        execucao_id = criado.json()["execucao_id"]
        trajeto = _trajeto(args.labirinto)
        meta_x, meta_y = _meta(args.labirinto)
        indice_trajeto = 0
        print(f"execucao_id={execucao_id}  labirinto={args.labirinto}  boot={boot}")
        print(f"meta=({meta_x},{meta_y})  passos no trajeto={len(trajeto)}")
        print(f"intervalo={args.intervalo}s  ticks={args.ticks}")
        _imprimir_cabecalho()

        seq, t_ms, continuar = _simular_health_check(
            client, execucao_id, args, boot=boot, seq=seq, t_ms=t_ms
        )
        if not continuar:
            url_detalhe = f"{args.url.rstrip('/')}/execucoes/{execucao_id}"
            print(f"\nSem corrida (HC reprovado). Detalhe: GET {url_detalhe}")
            return 0

        for tick in range(args.ticks):
            if tick > 0 or args.intervalo > 0:
                time.sleep(args.intervalo)

            seq += 1
            t_ms += int(args.intervalo * 1000)
            bat_mv = max(6000, args.bat_inicial - tick * args.queda_mv)
            cx, cy = _celula_atual(trajeto, indice_trajeto)
            linha = _tel(
                boot,
                seq,
                t_ms,
                x=cx,
                y=cy,
                bat_mv=bat_mv,
                vel_mm_s=args.vel_mm_s,
            )
            resp = _post_telemetria(client, linha)
            if resp.status_code != 200:
                print(f"tick {tick} tel: {resp.status_code} {resp.text}", file=sys.stderr)
                return 1

            detalhe = client.get(f"/execucoes/{execucao_id}").json()
            _imprimir_linha(tick, seq, bat_mv, _ultima_tentativa(detalhe), f"tel ({cx},{cy})")

            if (
                args.passo_a_cada > 0
                and (tick + 1) % args.passo_a_cada == 0
                and indice_trajeto < len(trajeto) - 1
            ):
                indice_trajeto += 1
                seq += 1
                t_ms += 50
                _, _, resp = _enviar_passo_trajeto(
                    client,
                    execucao_id,
                    boot=boot,
                    seq=seq,
                    t_ms=t_ms,
                    trajeto=trajeto,
                    indice=indice_trajeto,
                    tick=tick,
                )
                if resp.status_code != 200:
                    print(f"tick {tick} passo: {resp.status_code} {resp.text}", file=sys.stderr)
                    return 1

        pos_x, pos_y = _celula_atual(trajeto, indice_trajeto)

        if args.encerrar == "interrompida":
            ultima = (pos_x, pos_y)
            enc = client.post(
                f"/execucoes/{execucao_id}/encerrar-tentativa",
                json={
                    "motivo": args.motivo_encerramento,
                    "observacao": "simulador: interrupção manual no meio da execução",
                },
            )
            if enc.status_code != 200:
                print(
                    f"encerrar-tentativa: {enc.status_code} {enc.text}",
                    file=sys.stderr,
                )
                return 1
            detalhe = enc.json()
            tent = _ultima_tentativa(detalhe)
            _imprimir_linha(
                args.ticks,
                seq,
                None,
                tent,
                f"POST encerrar-tentativa ({args.motivo_encerramento})",
            )
            print(f"execucao.status={detalhe.get('status')}  (esperado: em_andamento)")

            time.sleep(args.intervalo)
            seq += 1
            t_ms += 100
            bat_mv = max(6000, args.bat_inicial - args.ticks * args.queda_mv)
            resp = _post_telemetria(
                client,
                _tel(
                    boot,
                    seq,
                    t_ms,
                    x=ultima[0],
                    y=ultima[1],
                    bat_mv=bat_mv,
                    vel_mm_s=args.vel_mm_s,
                ),
            )
            if resp.status_code != 200:
                print(f"tel pós-encerrar: {resp.status_code} {resp.text}", file=sys.stderr)
                return 1
            comandos = resp.json().get("comandos") or []
            print(f"comandos na resposta /telemetria: {comandos}")
            detalhe = client.get(f"/execucoes/{execucao_id}").json()
            _imprimir_linha(args.ticks, seq, bat_mv, _ultima_tentativa(detalhe), "tel (ponte)")

            if args.confirmar_firmware:
                seq += 1
                t_ms += 50
                resp = _post_telemetria(
                    client, _falha_web(boot, seq, t_ms, ultima[0], ultima[1])
                )
                if resp.status_code != 200:
                    print(f"falha web: {resp.status_code} {resp.text}", file=sys.stderr)
                    return 1
                print(f"comandos após falha web: {resp.json().get('comandos') or []}")
                _imprimir_linha(
                    args.ticks,
                    seq,
                    None,
                    _ultima_tentativa(client.get(f"/execucoes/{execucao_id}").json()),
                    "falha origem=web (confirmação)",
                )
        elif args.encerrar == "cancelada":
            print(
                f"\naguardando {args.espera_link_perdido:.1f}s sem telemetria "
                f"(simula link perdido; depois uma tel dispara verificar_monitores)…"
            )
            time.sleep(args.espera_link_perdido)
            seq += 1
            t_ms += int(args.espera_link_perdido * 1000)
            bat_mv = max(6000, args.bat_inicial - args.ticks * args.queda_mv)
            linha = _tel(
                boot,
                seq,
                t_ms,
                x=pos_x,
                y=pos_y,
                bat_mv=bat_mv,
                vel_mm_s=args.vel_mm_s,
            )
            resp = _post_telemetria(client, linha)
            if resp.status_code != 200:
                print(f"tel pós-link: {resp.status_code} {resp.text}", file=sys.stderr)
                return 1
            detalhe = client.get(f"/execucoes/{execucao_id}").json()
            _imprimir_linha(
                args.ticks, seq, bat_mv, _ultima_tentativa(detalhe), "tel (link_lost → cancelada)"
            )
            print(f"execucao.status={detalhe.get('status')}")
        elif args.encerrar != "nenhum":
            if args.encerrar == "sucesso":
                if indice_trajeto < len(trajeto) - 1:
                    print("\ncompletando passos até a meta antes do sucesso…")
                    try:
                        seq, t_ms, indice_trajeto = _completar_trajeto_ate_meta(
                            client,
                            execucao_id,
                            args,
                            boot=boot,
                            seq=seq,
                            t_ms=t_ms,
                            trajeto=trajeto,
                            indice=indice_trajeto,
                            tick=args.ticks,
                        )
                    except RuntimeError:
                        return 1
                pos_x, pos_y = _celula_atual(trajeto, indice_trajeto)
                if (pos_x, pos_y) != (meta_x, meta_y):
                    print(
                        f"trajeto parou em ({pos_x},{pos_y}), meta é ({meta_x},{meta_y})",
                        file=sys.stderr,
                    )
                    return 1
            time.sleep(args.intervalo)
            seq += 1
            t_ms += 100
            if args.encerrar == "sucesso":
                linha = _sucesso(boot, seq, t_ms, meta_x, meta_y)
                rotulo = f"sucesso ({meta_x},{meta_y})"
            else:
                linha = _linha(
                    {
                        "v": 1,
                        "boot": boot,
                        "seq": seq,
                        "t_ms": t_ms,
                        "tipo": "falha",
                        "motivo": "collision",
                        "origem": "automatica",
                        "x": pos_x,
                        "y": pos_y,
                        "componente": None,
                    }
                )
                rotulo = f"falha ({pos_x},{pos_y})"
            resp = _post_telemetria(client, linha)
            if resp.status_code != 200:
                print(f"{rotulo}: {resp.status_code} {resp.text}", file=sys.stderr)
                return 1
            detalhe = client.get(f"/execucoes/{execucao_id}").json()
            _imprimir_linha(args.ticks, seq, None, _ultima_tentativa(detalhe), rotulo)
            print(f"execucao.status={detalhe.get('status')}")

        url_detalhe = f"{args.url.rstrip('/')}/execucoes/{execucao_id}"
        print(f"\nDetalhe completo: GET {url_detalhe}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
