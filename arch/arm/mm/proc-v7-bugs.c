
// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/smp.h>

#include <asm/cp15.h>
#include <asm/cputype.h>
#include <asm/system_misc.h>

#ifdef CONFIG_HARDEN_BRANCH_PREDICTOR
DEFINE_PER_CPU(harden_branch_predictor_fn_t, harden_branch_predictor_fn);

static void harden_branch_predictor_bpiall(void)
{
	write_sysreg(0, BPIALL);
}

static void harden_branch_predictor_iciallu(void)
{
	write_sysreg(0, ICIALLU);
}

static void cpu_v7_spectre_init(void)
{
	const char *spectre_v2_method = NULL;
	int cpu = smp_processor_id();

	if (per_cpu(harden_branch_predictor_fn, cpu))
		return;

	switch (read_cpuid_part()) {
	case ARM_CPU_PART_CORTEX_A8:
	case ARM_CPU_PART_CORTEX_A9:
	case ARM_CPU_PART_CORTEX_A12:
	case ARM_CPU_PART_CORTEX_A17:
	case ARM_CPU_PART_CORTEX_A73:
	case ARM_CPU_PART_CORTEX_A75:
		per_cpu(harden_branch_predictor_fn, cpu) =
			harden_branch_predictor_bpiall;
		spectre_v2_method = "BPIALL";
		break;

	case ARM_CPU_PART_CORTEX_A15:
	case ARM_CPU_PART_BRAHMA_B15:
		per_cpu(harden_branch_predictor_fn, cpu) =
			harden_branch_predictor_iciallu;
		spectre_v2_method = "ICIALLU";
		break;
	}
	if (spectre_v2_method)
		pr_info("CPU%u: Spectre v2: using %s workaround\n",
			smp_processor_id(), spectre_v2_method);
}
#else
static void cpu_v7_spectre_init(void)
{
}
#endif

static __maybe_unused bool cpu_v7_check_auxcr_set(bool *warned,
						  u32 mask, const char *msg)
{
	u32 aux_cr;

	asm("mrc p15, 0, %0, c1, c0, 1" : "=r" (aux_cr));

	if ((aux_cr & mask) != mask) {
		if (!*warned)
			pr_err("CPU%u: %s", smp_processor_id(), msg);
		*warned = true;
		return false;
	}
	return true;
}

static DEFINE_PER_CPU(bool, spectre_warned);

static bool check_spectre_auxcr(bool *warned, u32 bit)
{
	return IS_ENABLED(CONFIG_HARDEN_BRANCH_PREDICTOR) &&
		cpu_v7_check_auxcr_set(warned, bit,
				       "Spectre v2: firmware did not set auxiliary control register IBE bit, system vulnerable\n");
}

void cpu_v7_ca8_ibe(void)
{
	if (check_spectre_auxcr(this_cpu_ptr(&spectre_warned), BIT(6)))
		cpu_v7_spectre_init();
}

void cpu_v7_ca15_ibe(void)
{
	if (check_spectre_auxcr(this_cpu_ptr(&spectre_warned), BIT(0)))
		cpu_v7_spectre_init();
}

void cpu_v7_bugs_init(void)
{
	cpu_v7_spectre_init();
}
