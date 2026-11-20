#include "dip.h"

contrato_tipo_dip_t dip_tipo(uint8_t vias)
{
    switch (vias & 0x03u) {
    case 0:
        return CONTRATO_DIP_4X4;
    case 1:
        return CONTRATO_DIP_8X4;
    case 2:
        return CONTRATO_DIP_12X4;
    default:
        return CONTRATO_DIP_INVALIDO;
    }
}

nav_tipo_t dip_tipo_nav(uint8_t vias)
{
    switch (dip_tipo(vias)) {
    case CONTRATO_DIP_8X4:
        return NAV_TIPO_8X4;
    case CONTRATO_DIP_12X4:
        return NAV_TIPO_12X4;
    default:
        return NAV_TIPO_4X4;
    }
}
