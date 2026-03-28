#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "cortex_emu.h"

static void die(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

int main(void) {
    cortex_emu_t *emu = cortex_emu_create();
    if (!emu) die("cortex_emu_create returned NULL");

    /* Configure: Cortex-M3, flash at 0x08000000 (128KB), SRAM at 0x20000000 (8KB) */
    cortex_emu_configure(emu, CORTEX_EMU_CORE_M3,
                         0x08000000, 128 * 1024,
                         0x20000000, 8 * 1024,
                         48000000);

    /*
     * Flash image:
     *   Word 0 (offset 0x00): Initial SP = 0x20002000
     *   Word 1 (offset 0x04): Reset vector = 0x08000009 (entry at +8, Thumb)
     *   Word 2 (offset 0x08): 0x46C046C0  = two 16-bit NOPs (MOV R8,R8)
     *   Word 3 (offset 0x0C): 0xE7FEE7FE  = two 16-bit infinite-loop branches
     */
    cortex_emu_load_flash(emu, 0x00, 0x20002000u);  /* initial SP */
    cortex_emu_load_flash(emu, 0x04, 0x08000009u);  /* reset vector, Thumb bit set */
    cortex_emu_load_flash(emu, 0x08, 0x46C046C0u);  /* NOP; NOP */
    cortex_emu_load_flash(emu, 0x0C, 0xE7FEE7FEu);  /* B .; B . */

    /* Reset: loads SP from word 0, PC from word 1 */
    cortex_emu_reset(emu);

    /* After reset, SP should be 0x20002000 and PC should be 0x08000008 */
    uint32_t sp = cortex_emu_get_reg(emu, 13);
    uint32_t pc = cortex_emu_get_reg(emu, 15);
    printf("After reset: SP=0x%08X PC=0x%08X\n", sp, pc);
    if (sp != 0x20002000u) die("SP after reset wrong");
    if (pc != 0x08000008u) die("PC after reset wrong");

    /* Run 10 steps */
    for (int i = 0; i < 10; i++) {
        uint32_t cycles = cortex_emu_step(emu);
        if (cortex_emu_is_lockup(emu)) die("CPU entered lockup");
        if (cortex_emu_get_fault_count(emu) > 0) die("hard fault occurred");
        (void)cycles;
    }

    printf("PC after 10 steps: 0x%08X\n", cortex_emu_get_reg(emu, 15));
    printf("Hard fault count: %u\n", cortex_emu_get_fault_count(emu));

    if (cortex_emu_get_fault_count(emu) != 0) die("unexpected hard fault");

    cortex_emu_destroy(emu);
    printf("PASS\n");
    return 0;
}
