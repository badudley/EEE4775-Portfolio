# App 4 scaffold — Synchronization Quest (Part B)
# Engineering analysis prompts 

1. Mutex vs binary semaphore as lock — why the mutex? 
A mutex provides ownership and priority inheritance, while a binary semaphore does not. Ownership ensures that only the task which acquires the mutex can release it, preventing accidental unlocks by other tasks. Priority inheritance temporarily raises the priority of a lower-priority task holding the mutex when a higher-priority task is blocked waiting for it. This prevents unbounded priority inversion and ensures timely access to shared resources. Since multiple tasks modify the patient record, a mutex is the correct synchronization primitive. 

2. Counting semaphore size - how chosen? 
The capacity of 3 was selected to reflect a hard hardware constraint: the spacecraft possesses exactly 3 physical high-gain antenna modems. With 4 concurrent flight tasks generating telemetry, the 3-slot counting semaphore acts as a token bucket, granting immediate access to available channels and causing the 4th task to gracefully back off when capacity is saturated.

3. Priority inversion timeline step-by-step.
I (20) app4: ==== ODYSSEY-1 [App 4] starting  sync quest ====
I (20) app4: Lock mode: MUTEX (priority inheritance ON) (USE_PI_MUTEX=1)
I (30) app4: Induced-failure mode: INDUCE_FAILURE=0 (0=mutex guards telemetry_seq, 1=unguarded)
I (40) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (40) app4: [Attitude-Sensor] telemetry_seq 0 -> 1
I (50) app4: [Thermal-Sensor] telemetry_seq 1 -> 2
I (40) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (40) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (40) app4: [PI] inversion demo lock = MUTEX (priority inheritance ON)
I (80) app4: [PI][L=orbit_calc] took nav_lock @ 104799 us  running orbit-determination burn
I (130) app4: [PI][H=fault_handler] attitude fault raised @ 156761 us  needs nav_lock, blocking
W (12350) app4: [PI][H=fault_handler] ACQUIRED @ 12379050 us  waited 12222289 us (~12222 ms)  [lock=MUTEX (priority inheritance ON)]
I (12360) app4: [PI][M=star_tracker] ready @ 12382382 us  calibrating (takes no lock)
I (36910) app4: [PI][M=star_tracker] done  @ 36933735 us (ran 24551353 us wall-clock)
I (36910) app4: [Thermal-Sensor] telemetry_seq 2 -> 3
I (36910) app4: [Attitude-Sensor] telemetry_seq 3 -> 4
I (36920) app4: [PI][L=orbit_calc] released nav_lock @ 12379008 us (held 12274209 us wall-clock)
I (36920) app4: [thruster] Collision-Avoidance released its RCS valve
I (36920) app4: [thruster] Docking-Alignment claimed an RCS valve  firing burn...
I (36920) app4: [thruster] Station-Keeping released its RCS valve
I (36920) app4: [thruster] Attitude-Control released its RCS valve
W (36960) app4: [MONITOR] attempts=4  telemetry_seq=4  lost_updates=0  (INDUCE_FAILURE=0)
I (37030) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (37050) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (37140) app4: [Attitude-Sensor] telemetry_seq 4 -> 5
I (37200) app4: [Thermal-Sensor] telemetry_seq 5 -> 6
I (37360) app4: [Attitude-Sensor] telemetry_seq 6 -> 7
I (37490) app4: [Thermal-Sensor] telemetry_seq 7 -> 8
I (37580) app4: [Attitude-Sensor] telemetry_seq 8 -> 9
I (37780) app4: [Thermal-Sensor] telemetry_seq 9 -> 10
I (37800) app4: [Attitude-Sensor] telemetry_seq 10 -> 11
I (37950) app4: [thruster] Station-Keeping released its RCS valve
I (37950) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (38020) app4: [Attitude-Sensor] telemetry_seq 11 -> 12
I (38070) app4: [Thermal-Sensor] telemetry_seq 12 -> 13
I (38130) app4: [thruster] Collision-Avoidance released its RCS valve
I (38130) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (38240) app4: [Attitude-Sensor] telemetry_seq 13 -> 14
I (38240) app4: [thruster] Docking-Alignment released its RCS valve
I (38240) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (38360) app4: [Thermal-Sensor] telemetry_seq 14 -> 15
I (38460) app4: [Attitude-Sensor] telemetry_seq 15 -> 16
I (38650) app4: [Thermal-Sensor] telemetry_seq 16 -> 17
I (38660) app4: [thruster] Attitude-Control released its RCS valve
I (38660) app4: [thruster] Docking-Alignment claimed an RCS valve  firing burn...
I (38680) app4: [Attitude-Sensor] telemetry_seq 17 -> 18
I (38900) app4: [Attitude-Sensor] telemetry_seq 18 -> 19
I (38940) app4: [Thermal-Sensor] telemetry_seq 19 -> 20
I (39040) app4: [thruster] Station-Keeping released its RCS valve
I (39040) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (39120) app4: [Attitude-Sensor] telemetry_seq 20 -> 21
I (39230) app4: [Thermal-Sensor] telemetry_seq 21 -> 22
I (39340) app4: [Attitude-Sensor] telemetry_seq 22 -> 23
I (39350) app4: [thruster] Collision-Avoidance released its RCS valve
I (39350) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (39520) app4: [Thermal-Sensor] telemetry_seq 23 -> 24
I (39560) app4: [Attitude-Sensor] telemetry_seq 24 -> 25
I (39750) app4: [thruster] Attitude-Control released its RCS valve
I (39750) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (39780) app4: [Attitude-Sensor] telemetry_seq 25 -> 26
I (39810) app4: [Thermal-Sensor] telemetry_seq 26 -> 27
I (39970) app4: [thruster] Docking-Alignment released its RCS valve
I (39970) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (40000) app4: [Attitude-Sensor] telemetry_seq 27 -> 28
I (40100) app4: [Thermal-Sensor] telemetry_seq 28 -> 29
I (40220) app4: [Attitude-Sensor] telemetry_seq 29 -> 30
I (40260) app4: [thruster] Station-Keeping released its RCS valve
I (40260) app4: [thruster] Docking-Alignment claimed an RCS valve  firing burn...
I (40390) app4: [Thermal-Sensor] telemetry_seq 30 -> 31
I (40440) app4: [Attitude-Sensor] telemetry_seq 31 -> 32
I (40660) app4: [Attitude-Sensor] telemetry_seq 32 -> 33
I (40680) app4: [Thermal-Sensor] telemetry_seq 33 -> 34
I (40680) app4: [thruster] Attitude-Control released its RCS valve
I (40680) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (40860) app4: [thruster] Collision-Avoidance released its RCS valve
I (40860) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (40880) app4: [Attitude-Sensor] telemetry_seq 34 -> 35
I (40970) app4: [Thermal-Sensor] telemetry_seq 35 -> 36
I (41100) app4: [Attitude-Sensor] telemetry_seq 36 -> 37
I (41260) app4: [Thermal-Sensor] telemetry_seq 37 -> 38
I (41320) app4: [Attitude-Sensor] telemetry_seq 38 -> 39
I (41540) app4: [Attitude-Sensor] telemetry_seq 39 -> 40
I (41550) app4: [Thermal-Sensor] telemetry_seq 40 -> 41
I (41570) app4: [thruster] Docking-Alignment released its RCS valve
I (41570) app4: [thruster] Attitude-Control released its RCS valve
I (41570) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (41590) app4: [thruster] Station-Keeping released its RCS valve
I (41670) app4: [thruster] Docking-Alignment claimed an RCS valve  firing burn...
I (41670) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (41760) app4: [Attitude-Sensor] telemetry_seq 41 -> 42
I (41840) app4: [Thermal-Sensor] telemetry_seq 42 -> 43
W (41970) app4: [MONITOR] attempts=43  telemetry_seq=43  lost_updates=0  (INDUCE_FAILURE=0)
I (41980) app4: [Attitude-Sensor] telemetry_seq 43 -> 44
I (42130) app4: [Thermal-Sensor] telemetry_seq 44 -> 45
I (42200) app4: [Attitude-Sensor] telemetry_seq 45 -> 46
I (42380) app4: [thruster] Attitude-Control released its RCS valve
I (42380) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (42420) app4: [Thermal-Sensor] telemetry_seq 46 -> 47
I (42420) app4: [Attitude-Sensor] telemetry_seq 47 -> 48
I (42640) app4: [Attitude-Sensor] telemetry_seq 48 -> 49
I (42680) app4: [thruster] Collision-Avoidance released its RCS valve
I (42680) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (42710) app4: [Thermal-Sensor] telemetry_seq 49 -> 50
I (42860) app4: [Attitude-Sensor] telemetry_seq 50 -> 51
I (42970) app4: [thruster] Docking-Alignment released its RCS valve
I (42970) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (43000) app4: [Thermal-Sensor] telemetry_seq 51 -> 52
I (43080) app4: [Attitude-Sensor] telemetry_seq 52 -> 53
I (43290) app4: [Thermal-Sensor] telemetry_seq 53 -> 54
I (43290) app4: [thruster] Station-Keeping released its RCS valve
I (43290) app4: [thruster] Docking-Alignment claimed an RCS valve  firing burn...
I (43300) app4: [Attitude-Sensor] telemetry_seq 54 -> 55
I (43390) app4: [thruster] Attitude-Control released its RCS valve
I (43390) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (43530) app4: [Attitude-Sensor] telemetry_seq 55 -> 56
I (43580) app4: [Thermal-Sensor] telemetry_seq 56 -> 57
I (43750) app4: [Attitude-Sensor] telemetry_seq 57 -> 58
I (43870) app4: [Thermal-Sensor] telemetry_seq 58 -> 59
I (43970) app4: [Attitude-Sensor] telemetry_seq 59 -> 60
I (44080) app4: [thruster] Collision-Avoidance released its RCS valve
I (44080) app4: [thruster] Attitude-Control claimed an RCS valve  firing burn...
I (44160) app4: [Thermal-Sensor] telemetry_seq 60 -> 61
I (44190) app4: [Attitude-Sensor] telemetry_seq 61 -> 62
I (44300) app4: [thruster] Station-Keeping released its RCS valve
I (44300) app4: [thruster] Collision-Avoidance claimed an RCS valve  firing burn...
I (44410) app4: [Attitude-Sensor] telemetry_seq 62 -> 63
I (44450) app4: [Thermal-Sensor] telemetry_seq 63 -> 64
I (44610) app4: [thruster] Docking-Alignment released its RCS valve
I (44610) app4: [thruster] Station-Keeping claimed an RCS valve  firing burn...
I (44630) app4: [Attitude-Sensor] telemetry_seq 64 -> 65
I (44740) app4: [Thermal-Sensor] telemetry_seq 65 -> 66
I (44790) app4: [thruster] Attitude-Control released its RCS valve
I (44790) app4: [thruster] Docking-Alignment claimed an RCS valve  firing burn...
I (44850) app4: [Attitude-Sensor] telemetry_seq 66 -> 67
I (45030) app4: [Thermal-Sensor] telemetry_seq 67 -> 68
I (45070) app4: [Attitude-Sensor] telemetry_seq 68 -> 69
I (45290) app4: [Attitude-Sensor] telemetry_seq 69 -> 70
I (45320) app4: [Thermal-Sensor] telemetry_seq 70 -> 7

4. Induced failure — deterministic or only sometimes?
Removing the mutex allows both writer tasks to access the shared patient record concurrently. Because each task independently reads, increments, and writes the variable, simultaneous execution can overwrite updates and produce incorrect values. The failure is non-deterministic because it depends on task scheduling. Some executions may appear correct, while others show duplicate values or skipped increments, demonstrating a classic race condition caused by unsynchronized shared data access. 