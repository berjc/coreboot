/*
 * This file is part of the coreboot project.
 *
 * DDR3 mem setup file for EXYNOS5 based board
 *
 * Copyright (C) 2012 Samsung Electronics
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <console/console.h>
#include <delay.h>
#include <arch/io.h>
#include "dmc.h"
#include "setup.h"
#include "cpu.h"
#include "clk.h"

#define TIMEOUT	10000

/* 'reset' field is currently ignored. */

int ddr3_mem_ctrl_init(struct mem_timings *mem, int interleave_size, int reset)
{
	struct exynos5420_clock *clk =
		(struct exynos5420_clock *)EXYNOS5_CLOCK_BASE;
	struct exynos5_phy_control *phy0_ctrl, *phy1_ctrl;
	struct exynos5_dmc *drex0, *drex1;
	struct exynos5_tzasc *tzasc0, *tzasc1;
	u32 val, nLockR, nLockW_phy0, nLockW_phy1;
	int i, chip;

	phy0_ctrl = (struct exynos5_phy_control *)EXYNOS5_DMC_PHY0_BASE;
	phy1_ctrl = (struct exynos5_phy_control *)EXYNOS5_DMC_PHY1_BASE;
	drex0 = (struct exynos5_dmc *)EXYNOS5420_DMC_DREXI_0;
	drex1 = (struct exynos5_dmc *)EXYNOS5420_DMC_DREXI_1;
	tzasc0 = (struct exynos5_tzasc *)EXYNOS5420_DMC_TZASC_0;
	tzasc1 = (struct exynos5_tzasc *)EXYNOS5420_DMC_TZASC_1;

	/* Enable PAUSE for DREX */
	setbits_le32(&clk->pause, ENABLE_BIT);

	/* Enable BYPASS mode */
	setbits_le32(&clk->bpll_con1, BYPASS_EN);

	writel(MUX_BPLL_SEL_FOUTBPLL, &clk->clk_src_cdrex);
	do {
		val = readl(&clk->clk_mux_stat_cdrex);
		val &= BPLL_SEL_MASK;
	} while (val != FOUTBPLL);

	clrbits_le32(&clk->bpll_con1, BYPASS_EN);

	/* Specify the DDR memory type as DDR3 */
	val = readl(&phy0_ctrl->phy_con0);
	val &= ~(PHY_CON0_CTRL_DDR_MODE_MASK << PHY_CON0_CTRL_DDR_MODE_SHIFT);
	val |= (DDR_MODE_DDR3 << PHY_CON0_CTRL_DDR_MODE_SHIFT);
	writel(val, &phy0_ctrl->phy_con0);

	val = readl(&phy1_ctrl->phy_con0);
	val &= ~(PHY_CON0_CTRL_DDR_MODE_MASK << PHY_CON0_CTRL_DDR_MODE_SHIFT);
	val |= (DDR_MODE_DDR3 << PHY_CON0_CTRL_DDR_MODE_SHIFT);
	writel(val, &phy1_ctrl->phy_con0);

	/* Set Read Latency and Burst Length for PHY0 and PHY1 */
	val = (mem->ctrl_bstlen << PHY_CON42_CTRL_BSTLEN_SHIFT) |
		(mem->ctrl_rdlat << PHY_CON42_CTRL_RDLAT_SHIFT);
	writel(val, &phy0_ctrl->phy_con42);
	writel(val, &phy1_ctrl->phy_con42);

	val = readl(&phy0_ctrl->phy_con26);
	val &= ~(T_WRDATA_EN_MASK << T_WRDATA_EN_OFFSET);
	val |= (T_WRDATA_EN_DDR3 << T_WRDATA_EN_OFFSET);
	writel(val, &phy0_ctrl->phy_con26);

	val = readl(&phy1_ctrl->phy_con26);
	val &= ~(T_WRDATA_EN_MASK << T_WRDATA_EN_OFFSET);
	val |= (T_WRDATA_EN_DDR3 << T_WRDATA_EN_OFFSET);
	writel(val, &phy1_ctrl->phy_con26);

	/* Set Driver strength for CK, CKE, CS & CA to 0x7
	 * Set Driver strength for Data Slice 0~3 to 0x6
	 */
	val = (0x7 << CA_CK_DRVR_DS_OFFSET) | (0x7 << CA_CKE_DRVR_DS_OFFSET) |
		(0x7 << CA_CS_DRVR_DS_OFFSET) | (0x7 << CA_ADR_DRVR_DS_OFFSET);
	val |= (0x7 << DA_3_DS_OFFSET) | (0x7 << DA_2_DS_OFFSET) |
		(0x7 << DA_1_DS_OFFSET) | (0x7 << DA_0_DS_OFFSET);
	writel(val, &phy0_ctrl->phy_con39);
	writel(val, &phy1_ctrl->phy_con39);

	/* ZQ Calibration */
	if (dmc_config_zq(mem, phy0_ctrl, phy1_ctrl))
		return SETUP_ERR_ZQ_CALIBRATION_FAILURE;

	clrbits_le32(&phy0_ctrl->phy_con16, ZQ_CLK_DIV_EN);
	clrbits_le32(&phy1_ctrl->phy_con16, ZQ_CLK_DIV_EN);

	/* DQ Signal */
	val = readl(&phy0_ctrl->phy_con14);
	val |= mem->phy0_pulld_dqs;
	writel(val, &phy0_ctrl->phy_con14);
	val = readl(&phy1_ctrl->phy_con14);
	val |= mem->phy1_pulld_dqs;
	writel(val, &phy1_ctrl->phy_con14);

	val = MEM_TERM_EN | PHY_TERM_EN;
	writel(val, &drex0->phycontrol0);
	writel(val, &drex1->phycontrol0);

	writel(mem->concontrol |
		(mem->dfi_init_start << CONCONTROL_DFI_INIT_START_SHIFT) |
		(mem->rd_fetch << CONCONTROL_RD_FETCH_SHIFT),
		&drex0->concontrol);
	writel(mem->concontrol |
		(mem->dfi_init_start << CONCONTROL_DFI_INIT_START_SHIFT) |
		(mem->rd_fetch << CONCONTROL_RD_FETCH_SHIFT),
		&drex1->concontrol);

	do {
		val = readl(&drex0->phystatus);
	} while ((val & DFI_INIT_COMPLETE) != DFI_INIT_COMPLETE);
	do {
		val = readl(&drex1->phystatus);
	} while ((val & DFI_INIT_COMPLETE) != DFI_INIT_COMPLETE);

	clrbits_le32(&drex0->concontrol, DFI_INIT_START);
	clrbits_le32(&drex1->concontrol, DFI_INIT_START);

	update_reset_dll(drex0, DDR_MODE_DDR3);
	update_reset_dll(drex1, DDR_MODE_DDR3);

	/* MEMBASECONFIG0 (CS0) */
	writel(mem->membaseconfig0, &tzasc0->membaseconfig0);
	writel(mem->membaseconfig0, &tzasc1->membaseconfig0);

	/* MEMBASECONFIG1 (CS1) */
	if (mem->chips_per_channel == 2) {
		writel(mem->membaseconfig1, &tzasc0->membaseconfig1);
		writel(mem->membaseconfig1, &tzasc1->membaseconfig1);
	}

	/* Memory Channel Inteleaving Size
	 * Exynos5420 Channel interleaving = 128 bytes
	 */
	/* MEMCONFIG0/1 */
	writel(mem->memconfig, &tzasc0->memconfig0);
	writel(mem->memconfig, &tzasc1->memconfig0);
	writel(mem->memconfig, &tzasc0->memconfig1);
	writel(mem->memconfig, &tzasc1->memconfig1);

	/* Precharge Configuration */
	writel(mem->prechconfig_tp_cnt << PRECHCONFIG_TP_CNT_SHIFT,
		&drex0->prechconfig0);
	writel(mem->prechconfig_tp_cnt << PRECHCONFIG_TP_CNT_SHIFT,
		&drex1->prechconfig0);

	/* TimingRow, TimingData, TimingPower and Timingaref
	 * values as per Memory AC parameters
	 */
	writel(mem->timing_ref, &drex0->timingref);
	writel(mem->timing_ref, &drex1->timingref);
	writel(mem->timing_row, &drex0->timingrow);
	writel(mem->timing_row, &drex1->timingrow);
	writel(mem->timing_data, &drex0->timingdata);
	writel(mem->timing_data, &drex1->timingdata);
	writel(mem->timing_power, &drex0->timingpower);
	writel(mem->timing_power, &drex1->timingpower);

	if (reset) {
		/* Send NOP, MRS and ZQINIT commands.
		 * Sending MRS command will reset the DRAM. We should not be
		 * reseting the DRAM after resume, this will lead to memory
		 * corruption as DRAM content is lost after DRAM reset.
		 */
		dmc_config_mrs(mem, drex0);
		dmc_config_mrs(mem, drex1);
	} else {
		u32 ret;

		/*
		 * During Suspend-Resume & S/W-Reset, as soon as PMU releases
		 * pad retention, CKE goes high. This causes memory contents
		 * not to be retained during DRAM initialization. Therfore,
		 * there is a new control register(0x100431e8[28]) which lets us
		 * release pad retention and retain the memory content until the
		 * initialization is complete.
		 */
		write32(PAD_RETENTION_DRAM_COREBLK_VAL,
			(void *)PAD_RETENTION_DRAM_COREBLK_OPTION);
		do {
			ret = read32((void *)PAD_RETENTION_DRAM_STATUS);
		} while (ret != 0x1);

		/*
		 * CKE PAD retention disables DRAM self-refresh mode.
		 * Send auto refresh command for DRAM refresh.
		 */
		for (i = 0; i < 128; i++) {
			for (chip = 0; chip < mem->chips_to_configure; chip++) {
				writel(DIRECT_CMD_REFA |
					(chip << DIRECT_CMD_CHIP_SHIFT),
					&drex0->directcmd);
				writel(DIRECT_CMD_REFA |
					(chip << DIRECT_CMD_CHIP_SHIFT),
					&drex1->directcmd);
			}
		}
	}

	if (mem->gate_leveling_enable) {

		writel(PHY_CON0_RESET_VAL, &phy0_ctrl->phy_con0);
		writel(PHY_CON0_RESET_VAL, &phy1_ctrl->phy_con0);

		setbits_le32(&phy0_ctrl->phy_con0, P0_CMD_EN);
		setbits_le32(&phy1_ctrl->phy_con0, P0_CMD_EN);

		val = PHY_CON2_RESET_VAL;
		val |= INIT_DESKEW_EN;
		writel(val, &phy0_ctrl->phy_con2);
		writel(val, &phy1_ctrl->phy_con2);

		val =  readl(&phy0_ctrl->phy_con1);
		val |= (RDLVL_PASS_ADJ_VAL << RDLVL_PASS_ADJ_OFFSET);
		writel(val, &phy0_ctrl->phy_con1);

		val =  readl(&phy1_ctrl->phy_con1);
		val |= (RDLVL_PASS_ADJ_VAL << RDLVL_PASS_ADJ_OFFSET);
		writel(val, &phy1_ctrl->phy_con1);

		nLockR = readl(&phy0_ctrl->phy_con13);
		nLockW_phy0 = (nLockR & CTRL_LOCK_COARSE_MASK) >> 2;
		nLockR = readl(&phy0_ctrl->phy_con12);
		nLockR &= ~CTRL_DLL_ON;
		nLockR |= nLockW_phy0;
		writel(nLockR, &phy0_ctrl->phy_con12);

		nLockR = readl(&phy1_ctrl->phy_con13);
		nLockW_phy1 = (nLockR & CTRL_LOCK_COARSE_MASK) >> 2;
		nLockR = readl(&phy1_ctrl->phy_con12);
		nLockR &= ~CTRL_DLL_ON;
		nLockR |= nLockW_phy1;
		writel(nLockR, &phy1_ctrl->phy_con12);

		val = (0x3 << DIRECT_CMD_BANK_SHIFT) | 0x4;
		for (chip = 0; chip < mem->chips_to_configure; chip++) {
			writel(val | (chip << DIRECT_CMD_CHIP_SHIFT),
				&drex0->directcmd);
			writel(val | (chip << DIRECT_CMD_CHIP_SHIFT),
				&drex1->directcmd);
		}

		setbits_le32(&phy0_ctrl->phy_con2, RDLVL_GATE_EN);
		setbits_le32(&phy1_ctrl->phy_con2, RDLVL_GATE_EN);

		setbits_le32(&phy0_ctrl->phy_con0, CTRL_SHGATE);
		setbits_le32(&phy1_ctrl->phy_con0, CTRL_SHGATE);

		val = readl(&phy0_ctrl->phy_con1);
		val &= ~(CTRL_GATEDURADJ_MASK);
		writel(val, &phy0_ctrl->phy_con1);

		val = readl(&phy1_ctrl->phy_con1);
		val &= ~(CTRL_GATEDURADJ_MASK);
		writel(val, &phy1_ctrl->phy_con1);

		writel(CTRL_RDLVL_GATE_ENABLE, &drex0->rdlvl_config);
		i = TIMEOUT;
		while (((readl(&drex0->phystatus) & RDLVL_COMPLETE_CHO) !=
			RDLVL_COMPLETE_CHO) && (i > 0)) {
			/*
			 * TODO(waihong): Comment on how long this take to
			 * timeout
			 */
			udelay(1);
			i--;
		}
		if (!i)
			return SETUP_ERR_RDLV_COMPLETE_TIMEOUT;
		writel(CTRL_RDLVL_GATE_DISABLE, &drex0->rdlvl_config);

		writel(CTRL_RDLVL_GATE_ENABLE, &drex1->rdlvl_config);
		i = TIMEOUT;
		while (((readl(&drex1->phystatus) & RDLVL_COMPLETE_CHO) !=
			RDLVL_COMPLETE_CHO) && (i > 0)) {
			/*
			 * TODO(waihong): Comment on how long this take to
			 * timeout
			 */
			udelay(1);
			i--;
		}
		if (!i)
			return SETUP_ERR_RDLV_COMPLETE_TIMEOUT;
		writel(CTRL_RDLVL_GATE_DISABLE, &drex1->rdlvl_config);

		writel(0, &phy0_ctrl->phy_con14);
		writel(0, &phy1_ctrl->phy_con14);

		val = (0x3 << DIRECT_CMD_BANK_SHIFT);
		for (chip = 0; chip < mem->chips_to_configure; chip++) {
			writel(val | (chip << DIRECT_CMD_CHIP_SHIFT),
				&drex0->directcmd);
			writel(val | (chip << DIRECT_CMD_CHIP_SHIFT),
				&drex1->directcmd);
		}

		/* Common Settings for Leveling */
		val = PHY_CON12_RESET_VAL;
		writel((val + nLockW_phy0), &phy0_ctrl->phy_con12);
		writel((val + nLockW_phy1), &phy1_ctrl->phy_con12);

		setbits_le32(&phy0_ctrl->phy_con2, DLL_DESKEW_EN);
		setbits_le32(&phy1_ctrl->phy_con2, DLL_DESKEW_EN);
	}

	/* Send PALL command */
	dmc_config_prech(mem, drex0);
	dmc_config_prech(mem, drex1);

	writel(mem->memcontrol, &drex0->memcontrol);
	writel(mem->memcontrol, &drex1->memcontrol);

	/*
	 * Set DMC Concontrol: Enable auto-refresh counter, provide
	 * read data fetch cycles and enable DREX auto set powerdown
	 * for input buffer of I/O in none read memory state.
	 */
	writel(mem->concontrol | (mem->aref_en << CONCONTROL_AREF_EN_SHIFT) |
		(mem->rd_fetch << CONCONTROL_RD_FETCH_SHIFT)|
		DMC_CONCONTROL_IO_PD_CON(0x2),
		&drex0->concontrol);
	writel(mem->concontrol | (mem->aref_en << CONCONTROL_AREF_EN_SHIFT) |
		(mem->rd_fetch << CONCONTROL_RD_FETCH_SHIFT)|
		DMC_CONCONTROL_IO_PD_CON(0x2),
		&drex1->concontrol);

	/* Enable Clock Gating Control for DMC
	 * this saves around 25 mw dmc power as compared to the power
	 * consumption without these bits enabled
	 */
	setbits_le32(&drex0->cgcontrol, DMC_INTERNAL_CG);
	setbits_le32(&drex1->cgcontrol, DMC_INTERNAL_CG);

	return 0;
}
