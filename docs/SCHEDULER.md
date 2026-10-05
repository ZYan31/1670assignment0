# Scheduler Write-up

  ## Algorithm
  I implemented the third option, priority scheduling with aging. This scheduler was taken with inspiration from Linux 2.4's original scheduler - https://github.com/zavg/linux-0.01/blob/master/kernel/sched.c#L68. 
  The states added to struct proc were:
  PRIORITY_SCALE (how much priority should be weighted over aging)
  priority (set at alloc = (pid+1)*PRIORITY_SCALE)
  AGING_RATE (how much weight should be given to aging)
  age (increases by AGING_RATE each timer tick)

  The algorithm generally works by computing an "effective priority", which is the priority + age. The scheduler selects the process with the highest effective priority. Each timer tick, all RUNNABLE processes increase age by AGING_RATE. When a process is RUNNING, age is set to 0.

  Quanta of tick/time is 10 milliseconds. 

  ## What it optimizes / gives up
  Priority scheduling with aging optimizes the response time for high priority tasks. Tasks with high priority will run almost immediately and will take a lot of the CPU time. Aging balances the priority slightly by adding a no-starvation guarantee. 
  
  This design gives up fairness, as the task with highest priority will run a lot of the time. If the task is really intensive, such as primecheck, it will end up dominating the CPU runtime. 

  ## Evidence
  Per-process CPU workload/time between the 2 scheduling methods: Priority + Aging vs. Round Robin over 10 seconds. 

PRIORITY + AGING (SCHED_RR 0)
Hello world from Duckie!
Process table:
pid 0  "hello"  load 0x100000  entry 0x100000  RUNNABLE
pid 1  "counter"  load 0x110000  entry 0x110000  RUNNABLE
pid 2  "primecheck"  load 0x120000  entry 0x120000  RUNNABLE
Starting pid 0 (hello)Process hello: before yield
primecheck: Found another 1000 primes; last one was 7919!
primecheck: Found another 1000 primes; last one was 17389!
Process counter: 0
Process hello: after yield
Process hello: before yield
Process counter: 1
primecheck: Found another 1000 primes; last one was 27449!
Process counter: 2
Process hello: after yield
Process hello: before yield
Process counter: 3
primecheck: Found another 1000 primes; last one was 37813!
Process counter: 4
Process hello: after yield
Process hello: before yield
Process counter: 5
primecheck: Found another 1000 primes; last one was 48611!
Process counter: 6
Process hello: after yield

ROUND-ROBIN (SCHED_RR 1):
Hello world from Duckie!
Process table:
pid 0  "hello"  load 0x100000  entry 0x100000  RUNNABLE
pid 1  "counter"  load 0x110000  entry 0x110000  RUNNABLE
pid 2  "primecheck"  load 0x120000  entry 0x120000  RUNNABLE
Starting pid 0 (hello)Process hello: before yield
Process counter: 0
Process hello: after yield
Process hello: before yield
Process counter: 1
Process hello: after yield
Process hello: before yield
Process counter: 2
Process hello: after yield
Process hello: before yield
Process counter: 3
primecheck: Found another 1000 primes; last one was 7919!
Process hello: after yield
Process hello: before yield
Process counter: 4
Process hello: after yield
Process hello: before yield
Process counter: 5
Process hello: after yield
Process hello: before yield

The different outputs were expected - for the priority + aging, when the primecheck started requiring >10 timer interrupts to output a line, the other processes began to show up. The priority + aging also prioritized runnign the highest priority program, which was primecheck, resulting in more lines being printed out from that process. 
For round robin scheduling, the other effect was also expected.Since the programs were prioritized equally, hello and counter programs both outputted a lot more lines than primecheck in the first 25 lines. 

CPU timer ticks/tracking percentages were attempted for 1000 ticks, but the programs seemed to have similar usages of cpu as primecheck dominated 999 ticks.
This may have been due to an issue with the quanta - setting the quanta to 10ms meant that hello and process switched away from their allocated timeshare before the tick could be recorded as theirs. Decreasing quanta did not change this as the 2 programs run in microseconds. When they switch away, primecheck dominates the rest of the quanta and is recorded as the main running process. 



  ## A workload where it's a bad choice
  When the workload is really high, a low-priority CPU job will barely get any runtime, especially if there is also a constant stream of high-priority tasks. The turnaround for a low priority task will increase significantly as compared to a round robin, which might complete the task very fast. Another example where it's a bad choice is if the highest-priority process does not finish running. 