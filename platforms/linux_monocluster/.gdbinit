define connect
    target remote localhost:2346
end

# useful macros for Linux
define dmesg
        set $__log_buf = $arg0
        set $log_start = $arg1
        set $log_end = $arg2
        set $x = $log_start
        echo "
        while ($x < $log_end)
                set $c = (char)(($__log_buf)[$x++])
                printf "%c" , $c
        end
        echo "\n
end
document dmesg
dmesg __log_buf log_start log_end
Print the content of the kernel message buffer
end

define thread_info
    set $th = (struct thread_info*)$gp
    echo "thread_info="
    print /x *$th
end
define task_struct
    set $th = (struct thread_info*)$gp
    set $ts = (struct task_struct*)$th->task
    echo "task_struct="
    print /x *$ts
end
define active_mm
    set $th = (struct thread_info*)$gp
    set $ts = (struct task_struct*)$th->task
    set $amm = (struct mm_struct*)$ts->active_mm
    echo "active_mm="
    print /x *$amm
end
define pgd
    set $th = (struct thread_info*)$gp
    set $ts = (struct task_struct*)$th->task
    set $ms = (struct mm_struct*)$ts->active_mm
    set $pg = (pgd_t*)$ms->pgd
    echo "pgd="
    print /x {pgd_t[2048]}$pg
end

define task_struct_header
    printf "Address      PID State     [User_EPC] [User-SP]  Kernel-SP  device comm\n"
end

define task_struct_show
    # task_struct addr and PID
    printf "0x%08X %5d", $arg0, $arg0->pid
    # Place a '<' marker on the current task
    #  if ($arg0 == current)
    # For PowerPC, register r2 points to the "current" task
    if ($arg0 == $r2)
        printf "<"
    else
        printf " "
    end
    # State
    if ($arg0->state == 0)
        printf "Running   "
    else
        if ($arg0->state == 1)
            printf "Sleeping  "
        else
            if ($arg0->state == 2)
                printf "Disksleep "
            else
                if ($arg0->state == 4)
                    printf "Zombie    "
                else
                    if ($arg0->state == 8)
                        printf "sTopped   "
                    else
                        if ($arg0->state == 16)
                            printf "Wpaging   "
                        else
                            printf "%2d       ", $arg0->state
                        end
                    end
                end
            end
        end
    end
    # User PC
    set $pt_regs = (struct pt_regs*)($arg0->stack + 0x2000 - sizeof(struct pt_regs))
    if ($pt_regs->cp0_epc)
        printf "0x%08X ", $pt_regs->cp0_epc
    else
        printf "           "
    end
    if ($pt_regs->regs[29])
        printf "0x%08X ", $pt_regs->regs[29]
    else
        printf "           "
    end
    # Display the kernel stack pointer
    set $th = (struct thread_info*)$arg0->stack
    printf "0x%08X ", $th->ksp
    # device
    if ($arg0->signal->tty)
        printf "%s   ", $arg0->signal->tty->name
    else
        printf "(none) "
    end
    # comm
    printf "%s\n", $arg0->comm
end

define find_next_task
    # Given a task address, find the next task in the linked list
    set $t = (struct task_struct *)$arg0
    set $offset=((char *)&$t->tasks - (char *)$t)
    set $t=(struct task_struct *)((char *)$t->tasks.next- (char *)$offset)
end

define ps
    # Print column headers
    task_struct_header
    set $t=&init_task
    task_struct_show $t
    find_next_task $t
    # Walk the list
    while &init_task!=$t
        # Display useful info about each task
        task_struct_show $t
        find_next_task $t
    end
end

