// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include <defs.h>
#include <memlayout.h>
#include <trap.h>
#include <mmu.h>
#include <x86.h>
#include <lapic.h>
#include <stdio.h>
#include <string.h>
#include <pmm.h>

volatile uint32_t *lapic;  // Initialized in mp.c

static void
lapic_w(int index, int value)
{
  lapic[index] = value;
  lapic[ID];  // wait for write to finish, by reading
}
//PAGEBREAK!

void
lapic_init(void)
{
  if(!lapic) 
    return;

  // Enable local APIC; set spurious interrupt vector.
  lapic_w(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // The timer repeatedly counts down at bus frequency
  // from lapic[TICR] and then issues an interrupt.  
  // If xv6 cared more about precise timekeeping,
  // TICR would be calibrated using an external time source.
  lapic_w(TDCR, X1);
  lapic_w(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapic_w(TICR, 10000000); 

  // Disable logical interrupt lines.
  lapic_w(LINT0, MASKED);
  lapic_w(LINT1, MASKED);

  // Disable performance counter overflow interrupts
  // on machines that provide that interrupt entry.
  if(((lapic[VER]>>16) & 0xFF) >= 4)
    lapic_w(PCINT, MASKED);

  // Map error interrupt to IRQ_ERROR.
  lapic_w(ERROR, T_IRQ0 + IRQ_ERROR);

  // Clear error status register (requires back-to-back writes).
  lapic_w(ESR, 0);
  lapic_w(ESR, 0);

  // Ack any outstanding interrupts.
  lapic_w(EOI, 0);

  // Send an Init Level De-Assert to synchronise arbitration ID's.
  lapic_w(ICRHI, 0);
  lapic_w(ICRLO, BCAST | INIT | LEVEL);
  while(lapic[ICRLO] & DELIVS)
    ;

  // Enable interrupts on the APIC (but not on the processor).
  lapic_w(TPR, 0);
}

int
cpu_num(void)
{
  // Cannot call cpu when interrupts are enabled:
  // result not guaranteed to last long enough to be used!
  // Would prefer to panic but even printing is chancy here:
  // almost everything, including cprintf and panic, calls cpu,
  // often indirectly through acquire and release.
  if(read_eflags()&FL_IF){
    static int32_t n;
    if(n++ == 0)
      cprintf("cpu called from %x with interrupts enabled\n",
        __builtin_return_address(0));
  }

  if(lapic)
    return lapic[ID]>>24;
  return 0;
}

// Acknowledge interrupt.
void
lapiceoi(void)
{
  if(lapic)
    lapic_w(EOI, 0);
}

// Spin for a given number of microseconds.
// On real hardware would want to tune this dynamically.
void
micro_delay(int us)
{
}

#define CMOS_PORT    0x70
#define CMOS_RETURN  0x71

// Start additional processor running entry code at addr.
// See Appendix B of MultiProcessor Specification.
void
lapic_startap(uint8_t apic_id, uint32_t addr)
{
  int32_t i;
  uint16_t *wrv;
  
  // "The BSP must initialize CMOS shutdown code to 0AH
  // and the warm reset vector (DWORD based at 40:67) to point at
  // the AP startup code prior to the [universal startup algorithm]."
  outb(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
  outb(CMOS_PORT+1, 0x0A);
  wrv = (uint16_t*)KADDR(0x40<<4 | 0x67);  // Warm reset vector
  wrv[0] = 0;
  wrv[1] = addr >> 4;

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  lapic_w(ICRHI, apic_id<<24);
  lapic_w(ICRLO, INIT | LEVEL | ASSERT);
  micro_delay(200);
  lapic_w(ICRLO, INIT | LEVEL);
  micro_delay(100);    // should be 10ms, but too slow in Bochs!
  
  // Send startup IPI (twice!) to enter code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for(i = 0; i < 2; i++){
    lapic_w(ICRHI, apic_id<<24);
    lapic_w(ICRLO, STARTUP | (addr>>12));
    micro_delay(200);
  }
}

#define CMOS_STATA   0x0a
#define CMOS_STATB   0x0b
#define CMOS_UIP    (1 << 7)        // RTC update in progress

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

static uint32_t cmos_read(uint32_t reg)
{
  outb(CMOS_PORT,  reg);
  micro_delay(200);

  return inb(CMOS_RETURN);
}

static void fill_rtcdate(struct rtc_date *r)
{
  r->second = cmos_read(SECS);
  r->minute = cmos_read(MINS);
  r->hour   = cmos_read(HOURS);
  r->day    = cmos_read(DAY);
  r->month  = cmos_read(MONTH);
  r->year   = cmos_read(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void cmos_time(struct rtc_date *r)
{
  struct rtc_date t1, t2;
  int32_t sb, bcd;

  sb = cmos_read(CMOS_STATB);

  bcd = (sb & (1 << 2)) == 0;

  // make sure CMOS doesn't modify time while we read it
  for (;;) {
    fill_rtcdate(&t1);
    if (cmos_read(CMOS_STATA) & CMOS_UIP)
        continue;
    fill_rtcdate(&t2);
    if (memcmp(&t1, &t2, sizeof(t1)) == 0)
      break;
  }

  // convert
  if (bcd) {
#define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
    CONV(second);
    CONV(minute);
    CONV(hour  );
    CONV(day   );
    CONV(month );
    CONV(year  );
#undef     CONV
  }

  *r = t1;
  r->year += 2000;
}
