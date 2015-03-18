// The I/O APIC manages hardware interrupts for an SMP system.
// http://www.intel.com/design/chipsets/datashts/29056601.pdf
// See also picirq.c.

#include <ioapic.h>
#include <mp.h>
#include <stdio.h>
#include <trap.h>

volatile struct ioapic *ioapic;

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
  uint32_t reg;
  uint32_t pad[3];
  uint32_t data;
};

static uint32_t
ioapic_read(int32_t reg)
{
  ioapic->reg = reg;
  return ioapic->data;
}

static void
ioapic_write(int32_t reg, uint32_t data)
{
  ioapic->reg = reg;
  ioapic->data = data;
}

void
ioapic_init(void)
{
  int32_t i, id, max_intr;

  if(!ismp)
    return;

  ioapic = (volatile struct ioapic*)IOAPIC;
  max_intr = (ioapic_read(REG_VER) >> 16) & 0xFF;
  id = ioapic_read(REG_ID) >> 24;
  if(id != ioapic_id)
    cprintf("ioapicinit: id isn't equal to ioapicid; not a MP\n");

  // Mark all interrupts edge-triggered, active high, disabled,
  // and not routed to any CPUs.
  for(i = 0; i <= max_intr; i++){
    ioapic_write(REG_TABLE+2*i, INT_DISABLED | (T_IRQ0 + i));
    ioapic_write(REG_TABLE+2*i+1, 0);
  }
}

void
ioapic_enable(int32_t irq, int32_t cpunum)
{
  if(!ismp)
    return;

  // Mark interrupt edge-triggered, active high,
  // enabled, and routed to the given cpunum,
  // which happens to be that cpu's APIC ID.
  ioapic_write(REG_TABLE+2*irq, T_IRQ0 + irq);
  ioapic_write(REG_TABLE+2*irq+1, cpunum << 24);
}
