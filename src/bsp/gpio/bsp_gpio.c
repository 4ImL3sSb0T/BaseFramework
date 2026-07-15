#include "bsp_gpio.h"
#include "main.h"

exit_code_t bsp_gpio_init(void)
{
    /* 引脚由 Core/Src/gpio.c MX_GPIO_Init 完成 */
    return EXIT_OK;
}

uint8_t bsp_gpio_key_level(uint8_t key_id)
{
    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0;

    switch ((bsp_key_id_t)key_id) {
    case BSP_KEY_UI_UP:
        port = UI_UP_GPIO_Port;
        pin  = UI_UP_Pin;
        break;
    case BSP_KEY_UI_DOWN:
        port = UI_DOWN_GPIO_Port;
        pin  = UI_DOWN_Pin;
        break;
    case BSP_KEY_UI_ENT:
        port = UI_ENT_GPIO_Port;
        pin  = UI_ENT_Pin;
        break;
    case BSP_KEY_UI_BACK:
        port = UI_BACK_GPIO_Port;
        pin  = UI_BACK_Pin;
        break;
    case BSP_KEY_USR:
        port = USR_KEY_GPIO_Port;
        pin  = USR_KEY_Pin;
        break;
    default:
        return (uint8_t)!BSP_KEY_ACTIVE_LEVEL;
    }

    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1u : 0u;
}

bool bsp_gpio_key_is_pressed(bsp_key_id_t key_id)
{
    return bsp_gpio_key_level((uint8_t)key_id) == BSP_KEY_ACTIVE_LEVEL;
}
