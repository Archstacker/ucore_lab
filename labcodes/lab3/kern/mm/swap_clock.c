#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <list.h>

static int
_clock_init_mm(struct mm_struct *mm)
{
     mm->sm_priv = NULL;
     //cprintf(" mm->sm_priv %x in clock_init_mm\n",mm->sm_priv);
     return 0;
}

static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *current=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);

    assert(entry != NULL);
    if(current == NULL) {
        list_init(entry);
        current = entry;
        mm->sm_priv = (void *)current;
    }
    else {
        list_add_before(current, entry);
    }
    return 0;
}

static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *current=(list_entry_t*) mm->sm_priv;
     assert(current != NULL);
     assert(in_tick==0);
     uintptr_t va = (le2page(current, pra_page_link))->pra_vaddr;
     pte_t *ptep = get_pte(mm->pgdir, va, 0);
     while(*ptep & PTE_A || *ptep & PTE_D) {
         if(!(*ptep & PTE_A)) {
             *ptep &= ~PTE_D;
         }
         *ptep &= ~PTE_A;
         tlb_invalidate(mm->pgdir, va);
         current = list_next(current);
         va = le2page(current, pra_page_link)->pra_vaddr;
         ptep = get_pte(mm->pgdir, va, 0);
     }
     mm->sm_priv = (void *)list_next(current);
     *ptr_page = le2page(current, pra_page_link);
     list_del(current);
     current=(list_entry_t*) mm->sm_priv;
     return 0;
}

static int
_clock_check_swap(void) {
    cprintf("write Virt Page c in clock_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==4);
    cprintf("write Virt Page a in clock_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==4);
    cprintf("write Virt Page d in clock_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==4);
    cprintf("write Virt Page b in clock_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==4);
    cprintf("write Virt Page e in clock_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    cprintf("write Virt Page b in clock_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==5);
    cprintf("write Virt Page a in clock_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==6);
    cprintf("write Virt Page b in clock_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==6);
    cprintf("write Virt Page c in clock_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==7);
    cprintf("write Virt Page d in clock_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==8);
    return 0;
}


static int
_clock_init(void)
{
    return 0;
}

static int
_clock_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_clock_tick_event(struct mm_struct *mm)
{ return 0; }


struct swap_manager swap_manager_clock =
{
     .name            = "clock swap manager",
     .init            = &_clock_init,
     .init_mm         = &_clock_init_mm,
     .tick_event      = &_clock_tick_event,
     .map_swappable   = &_clock_map_swappable,
     .set_unswappable = &_clock_set_unswappable,
     .swap_out_victim = &_clock_swap_out_victim,
     .check_swap      = &_clock_check_swap,
};
