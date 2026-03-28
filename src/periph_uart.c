#include "cortex_emu_internal.h"

int32_t emu_uart_index(uint32_t addr) {
    if (addr >= 0x40013800u && addr < 0x40013C00u) return 0;  /* UART1 */
    if (addr >= 0x40004400u && addr < 0x40004800u) return 1;  /* UART2 */
    return -1;
}

static uint32_t uart_base(int32_t idx) {
    return (idx == 0) ? 0x40013800u : 0x40004400u;
}

uint32_t emu_uart_read(cortex_emu_t *emu, uint32_t addr) {
    int32_t u = emu_uart_index(addr);
    if (u < 0) return 0;
    uint32_t reg = addr - uart_base(u);
    if (reg == 0x00) return emu->uart_cr1[u];
    if (reg == 0x04) return emu->uart_cr2[u];
    if (reg == 0x08) return emu->uart_cr3[u];
    if (reg == 0x0C) return emu->uart_brr[u];
    if (reg == 0x10) return emu->uart_gtpr[u];
    if (reg == 0x14) return emu->uart_rtor[u];
    if (reg == 0x18) return emu->uart_rqr[u];
    if (reg == 0x1C) return emu->uart_isr[u];
    if (reg == 0x20) return emu->uart_icr[u];
    if (reg == 0x24) {
        uint32_t val = emu->uart_rdr[u];
        emu->uart_isr[u] &= ~(1u << 5);  /* clear RXNE */
        return val;
    }
    if (reg == 0x28) return emu->uart_tdr[u];
    return 0;
}

void emu_uart_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    int32_t u = emu_uart_index(addr);
    if (u < 0) return;
    uint32_t reg = addr - uart_base(u);
    if (reg == 0x00) { emu->uart_cr1[u] = val; return; }
    if (reg == 0x04) { emu->uart_cr2[u] = val; return; }
    if (reg == 0x08) { emu->uart_cr3[u] = val; return; }
    if (reg == 0x0C) { emu->uart_brr[u] = val; return; }
    if (reg == 0x10) { emu->uart_gtpr[u] = val; return; }
    if (reg == 0x14) { emu->uart_rtor[u] = val; return; }
    if (reg == 0x18) { emu->uart_rqr[u] = val; return; }
    if (reg == 0x20) {
        emu->uart_isr[u] &= ~val;  /* ICR: write-1-to-clear */
        return;
    }
    if (reg == 0x28) {
        emu->uart_tdr[u] = val & 0x1FFu;
        /* Store in TX buffer */
        int32_t count = emu->uart_tx_count[u];
        if (count < UART_TX_BUF_SIZE) {
            int32_t head = emu->uart_tx_head[u];
            emu->uart_tx_buf[u * UART_TX_BUF_SIZE + head] = (uint8_t)(val & 0xFF);
            emu->uart_tx_head[u] = (head + 1) % UART_TX_BUF_SIZE;
            emu->uart_tx_count[u] = count + 1;
        }
        emu->uart_isr[u] |= (1u << 7) | (1u << 6);  /* TXE | TC */
        uint32_t cr1 = emu->uart_cr1[u];
        if ((cr1 & (1u << 7)) || (cr1 & (1u << 6))) {
            uint32_t irq = (u == 0) ? 27u : 28u;
            emu->nvic_pending |= (1u << irq);
        }
        return;
    }
}

void emu_uart_reset(cortex_emu_t *emu) {
    for (int u = 0; u < UART_COUNT; u++) {
        emu->uart_cr1[u] = 0;  emu->uart_cr2[u] = 0;
        emu->uart_cr3[u] = 0;  emu->uart_brr[u] = 0;
        emu->uart_gtpr[u] = 0; emu->uart_rtor[u] = 0;
        emu->uart_rqr[u] = 0;
        emu->uart_isr[u] = 0x000000C0u;  /* TXE=1, TC=1 at reset */
        emu->uart_icr[u] = 0; emu->uart_rdr[u] = 0;
        emu->uart_tdr[u] = 0;
        emu->uart_tx_head[u] = 0;
        emu->uart_tx_tail[u] = 0;
        emu->uart_tx_count[u] = 0;
    }
}
