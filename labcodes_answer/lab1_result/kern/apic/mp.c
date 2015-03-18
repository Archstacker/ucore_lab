// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mp.h>
#include <x86.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <pmm.h>
#include <lapic.h>
#include <proc.h>

struct cpu cpus[NCPU];
static struct cpu *bcpu;
int32_t ismp;
int32_t ncpu;
uint8_t ioapic_id;

int
mp_bcpu(void)
{
  return bcpu-cpus;
}

static uint8_t
sum(uint8_t *addr, int32_t len)
{
  int32_t i, sum;
  
  sum = 0;
  for(i=0; i<len; i++)
    sum += addr[i];
  return sum;
}

// Look for an MP structure in the len bytes at addr.
static struct mp*
mp_search1(uint32_t a, int len)
{
  uint8_t *e, *p, *addr;

  addr = KADDR(a);
  e = addr+len;
  for(p = addr; p < e; p += sizeof(struct mp))
    if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct mp*
mp_search(void)
{
  uint8_t *bda;
  uint32_t p;
  struct mp *mp;

  bda = (uint8_t *) KADDR(0x400);
  if((p = ((bda[0x0F]<<8)| bda[0x0E]) << 4)){
    if((mp = mp_search1(p, 1024)))
      return mp;
  } else {
    p = ((bda[0x14]<<8)|bda[0x13])*1024;
    if((mp = mp_search1(p-1024, 1024)))
      return mp;
  }
  return mp_search1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static struct mp_conf*
mp_config(struct mp **pmp)
{
  struct mp_conf *conf;
  struct mp *mp;

  if((mp = mp_search()) == 0 || mp->physaddr == 0)
    return 0;
  conf = (struct mp_conf*) KADDR((uint32_t) mp->physaddr);
  if(memcmp(conf, "PCMP", 4) != 0)
    return 0;
  if(conf->version != 1 && conf->version != 4)
    return 0;
  if(sum((uint8_t*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

void
mp_init(void)
{
  uint8_t *p, *e;
  struct mp *mp;
  struct mp_conf *conf;
  struct mp_proc *proc;
  struct mp_ioapic *ioapic;

  bcpu = &cpus[0];
  if((conf = mp_config(&mp)) == 0)
    return;
  ismp = 1;
  lapic = (uint32_t*)conf->lapicaddr;
  for(p=(uint8_t*)(conf+1), e=(uint8_t*)conf+conf->length; p<e; ){
    switch(*p){
    case MP_PROC:
      proc = (struct mp_proc*)p;
      if(ncpu != proc->apicid){
        cprintf("mp_init: ncpu=%d apicid=%d\n", ncpu, proc->apicid);
        ismp = 0;
      }
      if(proc->flags & MPBOOT)
        bcpu = &cpus[ncpu];
      cpus[ncpu].id = ncpu;
      ncpu++;
      p += sizeof(struct mp_proc);
      continue;
    case MP_IOAPIC:
      ioapic = (struct mp_ioapic*)p;
      ioapic_id = ioapic->apicno;
      p += sizeof(struct mp_ioapic);
      continue;
    case MP_BUS:
    case MP_IOINTR:
    case MP_LINTR:
      p += 8;
      continue;
    default:
      cprintf("mp_init: unknown config type %x\n", *p);
      ismp = 0;
    }
  }
  if(!ismp){
    // Didn't like what we found; fall back to no MP.
    ncpu = 1;
    lapic = 0;
    ioapic_id = 0;
    return;
  }

  if(mp->imcrp){
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
}
