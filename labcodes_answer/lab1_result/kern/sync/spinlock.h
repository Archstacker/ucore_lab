// Mutual exclusion lock.
struct spin_lock {
  uint32_t locked;       // Is the lock held?
  
  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint32_t pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};

void            acquire(struct spin_lock*);
void            get_caller_pcs(void*, uint32_t*);
int             holding(struct spin_lock*);
void            init_lock(struct spin_lock*, char*);
void            release(struct spin_lock*);
void            push_cli(void);
void            pop_cli(void);
