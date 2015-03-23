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
void            getcallerpcs(void*, uint32_t*);
int             holding(struct spin_lock*);
void            initlock(struct spin_lock*, uint8_t*);
void            release(struct spin_lock*);
void            pushcli(void);
void            popcli(void);
