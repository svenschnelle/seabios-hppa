// Basic x86 asm functions.
#ifndef __X86_H
#define __X86_H

#include "parisc/hppa_hardware.h"

// CPU flag bitdefs
#define F_CF (1<<0)
#define F_ZF (1<<6)
#define F_IF (1<<9)
#define F_ID (1<<21)

// CR0 flags
#define CR0_PG (1<<31) // Paging
#define CR0_CD (1<<30) // Cache disable
#define CR0_NW (1<<29) // Not Write-through
#define CR0_PE (1<<0)  // Protection enable

// PORT_A20 bitdefs
#define PORT_A20 0x0092
#define A20_ENABLE_BIT 0x02

#ifndef __ASSEMBLY__

#include "types.h" // u32
#include "byteorder.h" // le16_to_cpu

#define   PSW_I   0x00000001

static inline unsigned long arch_local_save_flags(void)
{
	unsigned long flags;
	asm volatile("ssm 0, %0" : "=r" (flags) : : "memory");
	return flags;
}

static inline void arch_local_irq_disable(void)
{
	asm volatile("rsm %0,%%r0\n" : : "i" (PSW_I) : "memory");
}

static inline void arch_local_irq_enable(void)
{
	asm volatile("ssm %0,%%r0\n" : : "i" (PSW_I) : "memory");
}

static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;
	asm volatile("rsm %1,%0" : "=r" (flags) : "i" (PSW_I) : "memory");
	return flags;
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	asm volatile("mtsm %0" : : "r" (flags) : "memory");
}

static inline void irq_disable(void)
{
   arch_local_irq_disable();
}

static inline void irq_enable(void)
{
    arch_local_irq_enable();
}

static inline u32 save_flags(void)
{
    return arch_local_irq_save();
}

static inline void restore_flags(u32 flags)
{
    arch_local_irq_restore(flags);
}



static inline void cpu_relax(void)
{
    asm volatile("nop": : :"memory");
}

static inline void nop(void)
{
    asm volatile("nop");
}

extern void hlt(void);

static inline void wbinvd(void)
{
    asm volatile("sync": : :"memory"); // fdc... FIXME !!! flush_data_cache_local
}



#define mfctl(reg)	({		\
	unsigned long cr;		\
	__asm__ __volatile__(		\
		"mfctl " #reg ",%0" :	\
		 "=r" (cr)		\
	);				\
	cr;				\
})

#define mtctl(gr, cr) \
	__asm__ __volatile__("mtctl %0,%1" \
		: /* no outputs */ \
		: "r" (gr), "i" (cr) : "memory")

/* these are here to de-mystefy the calling code, and to provide hooks */
/* which I needed for debugging EIEM problems -PB */
#define get_eiem() mfctl(15)
static inline void set_eiem(unsigned long val)
{
	mtctl(val, 15);
}

#define mfsp(reg)	({		\
	unsigned long cr;		\
	__asm__ __volatile__(		\
		"mfsp " #reg ",%0" :	\
		 "=r" (cr)		\
	);				\
	cr;				\
})

#define mtsp(val, cr) \
	{ if (__builtin_constant_p(val) && ((val) == 0)) \
	 __asm__ __volatile__("mtsp %%r0,%0" : : "i" (cr) : "memory"); \
	else \
	 __asm__ __volatile__("mtsp %0,%1" \
		: /* no outputs */ \
		: "r" (val), "i" (cr) : "memory"); }

#define CPUID_TSC (1 << 4)
#define CPUID_MSR (1 << 5)
#define CPUID_APIC (1 << 9)
#define CPUID_MTRR (1 << 12)
#define CPUID_X2APIC (1 << 21)
static inline void __cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
    asm("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (index));
}

static inline u32 cr0_read(void) {
    u32 cr0;
    asm("movl %%cr0, %0" : "=r"(cr0));
    return cr0;
}
static inline void cr0_write(u32 cr0) {
    asm("movl %0, %%cr0" : : "r"(cr0));
}
static inline void cr0_mask(u32 off, u32 on) {
    cr0_write((cr0_read() & ~off) | on);
}
static inline u16 cr0_vm86_read(void) {
    u16 cr0;
    asm("smsww %0" : "=r"(cr0));
    return cr0;
}

static inline u64 rdmsr(u32 index)
{
    u64 ret;
    asm ("rdmsr" : "=A"(ret) : "c"(index));
    return ret;
}

static inline void wrmsr(u32 index, u64 val)
{
    asm volatile ("wrmsr" : : "c"(index), "A"(val));
}

static inline unsigned long rdtscll(void)
{
    return mfctl(16);
}

static inline u32 __ffs(u32 x)
{
	unsigned long ret;

	if (!x)
		return 0;

	__asm__(
#ifdef CONFIG_64BIT
		" ldi       63,%1\n"
		" extrd,u,*<>  %0,63,32,%%r0\n"
		" extrd,u,*TR  %0,31,32,%0\n"	/* move top 32-bits down */
		" addi    -32,%1,%1\n"
#else
		" ldi       31,%1\n"
#endif
		" extru,<>  %0,31,16,%%r0\n"
		" extru,TR  %0,15,16,%0\n"	/* xxxx0000 -> 0000xxxx */
		" addi    -16,%1,%1\n"
		" extru,<>  %0,31,8,%%r0\n"
		" extru,TR  %0,23,8,%0\n"	/* 0000xx00 -> 000000xx */
		" addi    -8,%1,%1\n"
		" extru,<>  %0,31,4,%%r0\n"
		" extru,TR  %0,27,4,%0\n"	/* 000000x0 -> 0000000x */
		" addi    -4,%1,%1\n"
		" extru,<>  %0,31,2,%%r0\n"
		" extru,TR  %0,29,2,%0\n"	/* 0000000y, 1100b -> 0011b */
		" addi    -2,%1,%1\n"
		" extru,=  %0,31,1,%%r0\n"	/* check last bit */
		" addi    -1,%1,%1\n"
			: "+r" (x), "=r" (ret) );
	return ret;
}

static inline u32 __fls(u32 x)
{
	int ret;
	if (!x)
		return 0;

	__asm__(
	"	ldi		1,%1\n"
	"	extru,<>	%0,15,16,%%r0\n"
	"	zdep,TR		%0,15,16,%0\n"		/* xxxx0000 */
	"	addi		16,%1,%1\n"
	"	extru,<>	%0,7,8,%%r0\n"
	"	zdep,TR		%0,23,24,%0\n"		/* xx000000 */
	"	addi		8,%1,%1\n"
	"	extru,<>	%0,3,4,%%r0\n"
	"	zdep,TR		%0,27,28,%0\n"		/* x0000000 */
	"	addi		4,%1,%1\n"
	"	extru,<>	%0,1,2,%%r0\n"
	"	zdep,TR		%0,29,30,%0\n"		/* y0000000 (y&3 = 0) */
	"	addi		2,%1,%1\n"
	"	extru,=		%0,0,1,%%r0\n"
	"	addi		1,%1,%1\n"		/* if y & 8, add 1 */
		: "+r" (x), "=r" (ret) );

	return ret;
}

static inline u32 getesp(void) {
    u32 esp;
    asm("movl %%esp, %0" : "=rm"(esp));
    return esp;
}

static inline u32 rol(u32 val, u16 rol) {
    u32 res;
    asm volatile("roll %%cl, %%eax"
                 : "=a" (res) : "a" (val), "c" (rol));
    return res;
}

#define pci_ioport_addr(port) ((port >= 0x1000)  && (port < FIRMWARE_START))

static inline void outl(u32 value, portaddr_t port) {
    if (!pci_ioport_addr(port)) {
        *(volatile u32 *)(port) = be32_to_cpu(value);
    } else {
	/* write PCI I/O address to Dino's PCI_CONFIG_ADDR */
	outl(port, DINO_HPA + 0x064);
	/* write value to PCI_IO_DATA */
	outl(value, DINO_HPA + 0x06c);
    }
}

static inline void outw(u16 value, portaddr_t port) {
    if (!pci_ioport_addr(port)) {
        *(volatile u16 *)(port) = be16_to_cpu(value);
    } else {
	/* write PCI I/O address to Dino's PCI_CONFIG_ADDR */
	outl(port & ~3U, DINO_HPA + 0x064);
	/* write value to PCI_IO_DATA */
	outw(value, DINO_HPA + 0x06c + (port & 3));
    }
}

static inline void outb(u8 value, portaddr_t port) {
    if (!pci_ioport_addr(port)) {
	*(volatile u8 *)(port) = value;
    } else {
	/* write PCI I/O address to Dino's PCI_CONFIG_ADDR */
	outl(port & ~3U, DINO_HPA + 0x064);
	/* write value to PCI_IO_DATA */
	outb(value, DINO_HPA + 0x06c + (port & 3));
    }
}

static inline u8 inb(portaddr_t port) {
    if (!pci_ioport_addr(port)) {
        return *(volatile u8 *)(port);
    } else {
	/* write PCI I/O address to Dino's PCI_CONFIG_ADDR */
	outl(port & ~3U, DINO_HPA + 0x064);
	/* read value to PCI_IO_DATA */
	return inb(DINO_HPA + 0x06c + (port & 3));
    }
}

static inline u16 inw(portaddr_t port) {
    if (!pci_ioport_addr(port)) {
        return *(volatile u16 *)(port);
    } else {
	/* write PCI I/O address to Dino's PCI_CONFIG_ADDR */
	outl(port & ~3U, DINO_HPA + 0x064);
	/* read value to PCI_IO_DATA */
	return inw(DINO_HPA + 0x06c + (port & 3));
    }
}
static inline u32 inl(portaddr_t port) {
    if (!pci_ioport_addr(port)) {
        return *(volatile u32 *)(port);
    } else {
	/* write PCI I/O address to Dino's PCI_CONFIG_ADDR */
	outl(port, DINO_HPA + 0x064);
	/* read value to PCI_IO_DATA */
	return inl(DINO_HPA + 0x06c);
    }
}

static inline void insb(portaddr_t port, u8 *data, u32 count) {
    while (count--)
	*data++ = inb(port);
}
static inline void insw(portaddr_t port, u16 *data, u32 count) {
    while (count--)
	if (pci_ioport_addr(port))
		*data++ = be16_to_cpu(inw(port));
	else
		*data++ = inw(port);
}
static inline void insl(portaddr_t port, u32 *data, u32 count) {
    while (count--)
	if (pci_ioport_addr(port))
		*data++ = be32_to_cpu(inl(port));
	else
		*data++ = inl(port);
}
// XXX - outs not limited to es segment
static inline void outsb(portaddr_t port, u8 *data, u32 count) {
    while (count--)
	outb(*data++, port);
}
static inline void outsw(portaddr_t port, u16 *data, u32 count) {
    while (count--) {
	if (pci_ioport_addr(port))
		outw(cpu_to_be16(*data), port);
	else
		outw(*data, port);
	data++;
    }
}
static inline void outsl(portaddr_t port, u32 *data, u32 count) {
    while (count--) {
	if (pci_ioport_addr(port))
		outl(cpu_to_be32(*data), port);
	else
		outl(*data, port);
	data++;
    }
}

/* Compiler barrier is enough as an x86 CPU does not reorder reads or writes */
static inline void smp_rmb(void) {
    barrier();
}
static inline void smp_wmb(void) {
    barrier();
}

static inline void writel(void *addr, u32 val) {
    barrier();
    *(volatile u32 *)addr = val;
}
static inline void writew(void *addr, u16 val) {
    barrier();
    *(volatile u16 *)addr = val;
}
static inline void writeb(void *addr, u8 val) {
    barrier();
    *(volatile u8 *)addr = val;
}
static inline u32 readl(const void *addr) {
    u32 val = *(volatile const u32 *)addr;
    barrier();
    return val;
}
static inline u16 readw(const void *addr) {
    u16 val = *(volatile const u16 *)addr;
    barrier();
    return val;
}
static inline u8 readb(const void *addr) {
    u8 val = *(volatile const u8 *)addr;
    barrier();
    return val;
}

// GDT bits
#define GDT_CODE     (0x9bULL << 40) // Code segment - P,R,A bits also set
#define GDT_DATA     (0x93ULL << 40) // Data segment - W,A bits also set
#define GDT_B        (0x1ULL << 54)  // Big flag
#define GDT_G        (0x1ULL << 55)  // Granularity flag
// GDT bits for segment base
#define GDT_BASE(v)  ((((u64)(v) & 0xff000000) << 32)           \
                      | (((u64)(v) & 0x00ffffff) << 16))
// GDT bits for segment limit (0-1Meg)
#define GDT_LIMIT(v) ((((u64)(v) & 0x000f0000) << 32)   \
                      | (((u64)(v) & 0x0000ffff) << 0))
// GDT bits for segment limit (0-4Gig in 4K chunks)
#define GDT_GRANLIMIT(v) (GDT_G | GDT_LIMIT((v) >> 12))

struct descloc_s {
    u16 length;
    u32 addr;
} PACKED;

static inline void sgdt(struct descloc_s *desc) {
}
static inline void lgdt(struct descloc_s *desc) {
}

static inline u8 get_a20(void) {
    return 0;
}

static inline u8 set_a20(u8 cond) {
    return 0;
}

// x86.c
void cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx);

#endif // !__ASSEMBLY__

#endif // x86.h
