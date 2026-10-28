.text
.align 2
.arm

.global NNSi_SndArcLoadBank_hook
NNSi_SndArcLoadBank_hook:
ldr r5, =NNSi_SndArcLoadBank_return_address
mov r6, lr
str r6, [r5]
pop {r5-r6}
blx NNSi_SndArcLoadBank
ldr r1, =NNSi_SndArcLoadBank_return_address
ldr r1, [r1]
mov pc, r1

.pool

NNSi_SndArcLoadBank_return_address:
.word 0

@ romhack: the OAM loaders' hook (armhooks), ARM code: on to the Thumb function (src/rh_theme.c)
.global RH_LoadOamMain_arm
RH_LoadOamMain_arm:
ldr ip, =RH_LoadOamMain
bx ip

.pool

.global RH_LoadOamCopy_arm
RH_LoadOamCopy_arm:
ldr ip, =RH_LoadOamCopy
bx ip

.pool
