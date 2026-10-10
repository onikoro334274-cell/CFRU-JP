@ ipatix HQ Sound Mixer V2.1（https://github.com/ipatix）を pm-start/b2w2-reloaded（5705ed0）が配布バイナリから ASM に戻したもの（src/hq_mixer.S）をそのまま写した。
@ 2026-10-10 patches/0288: m4aSoundInit がこれを IWRAM 0x03005FA0 へ写して SoundMainRAM として呼ぶ（bytereplacement・repoints）。
@ 写すバイト数は 2,852（0x2C9 語）。中身を変えたら bytereplacement の 081C1078 も直す。devkitARM の as で組むと元の機械語と SHA256 一致（0a3b3eca…）。
/* Recovered from distributed HQ mixer machine code; original names unavailable. */
.syntax unified
.cpu arm7tdmi
.text
.balign 4
.global hq_mixer_start
hq_mixer_start:
.L_0000:
.thumb
    ldrb r3, [r0, #5] @ ROM +0x0000
.L_0002:
    lsrs r3, r3, #2 @ ROM +0x0002
.L_0004:
    beq .L_0088 @ ROM +0x0004
.L_0006:
    adr r1, .L_000c @ ROM +0x0006
.L_0008:
    bx r1 @ ROM +0x0008
.L_000a:
    mov r8, r8 @ ROM +0x000a
.L_000c:
.arm
    cmp r4, #2 @ ROM +0x000c
.L_0010:
    addeq r7, r0, #0x350 @ ROM +0x0010
.L_0014:
    addne r7, r5, r8 @ ROM +0x0014
.L_0018:
    mov r4, r8 @ ROM +0x0018
.L_001c:
    orr r3, r3, r3, lsl #16 @ ROM +0x001c
.L_0020:
    push {r8, lr} @ ROM +0x0020
.L_0024:
    ldr lr, [pc, #0x160] @ ROM +0x0024
.L_0028:
    ldrsb r0, [r5, r6] @ ROM +0x0028
.L_002c:
    ldrsb r1, [r5], #1 @ ROM +0x002c
.L_0030:
    ldrsb r2, [r7, r6] @ ROM +0x0030
.L_0034:
    ldrsb r8, [r7], #1 @ ROM +0x0034
.L_0038:
    ldrsb sb, [r5, r6] @ ROM +0x0038
.L_003c:
    ldrsb sl, [r5], #1 @ ROM +0x003c
.L_0040:
    ldrsb fp, [r7, r6] @ ROM +0x0040
.L_0044:
    ldrsb ip, [r7], #1 @ ROM +0x0044
.L_0048:
    add r0, r0, r1 @ ROM +0x0048
.L_004c:
    add r0, r0, r2 @ ROM +0x004c
.L_0050:
    adds r0, r0, r8 @ ROM +0x0050
.L_0054:
    addmi r0, r0, #4 @ ROM +0x0054
.L_0058:
    add r1, sb, sl @ ROM +0x0058
.L_005c:
    add r1, r1, fp @ ROM +0x005c
.L_0060:
    adds r1, r1, ip @ ROM +0x0060
.L_0064:
    addmi r1, r1, #4 @ ROM +0x0064
.L_0068:
    mul r0, r3, r0 @ ROM +0x0068
.L_006c:
    mul r1, r3, r1 @ ROM +0x006c
.L_0070:
    stm lr!, {r0, r1} @ ROM +0x0070
.L_0074:
    subs r4, r4, #2 @ ROM +0x0074
.L_0078:
    bgt .L_0028 @ ROM +0x0078
.L_007c:
    pop {r8, lr} @ ROM +0x007c
.L_0080:
    add r0, pc, #0x27 @ ROM +0x0080
.L_0084:
    bx r0 @ ROM +0x0084
.L_0088:
.thumb
    ldr r3, [pc, #0x100] @ ROM +0x0088
.L_008a:
    mov r1, r8 @ ROM +0x008a
.L_008c:
    movs r4, #0 @ ROM +0x008c
.L_008e:
    movs r5, #0 @ ROM +0x008e
.L_0090:
    movs r6, #0 @ ROM +0x0090
.L_0092:
    movs r7, #0 @ ROM +0x0092
.L_0094:
    lsrs r1, r1, #3 @ ROM +0x0094
.L_0096:
    blo .L_009a @ ROM +0x0096
.L_0098:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x0098
.L_009a:
    lsrs r1, r1, #1 @ ROM +0x009a
.L_009c:
    blo .L_00a2 @ ROM +0x009c
.L_009e:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x009e
.L_00a0:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x00a0
.L_00a2:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x00a2
.L_00a4:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x00a4
.L_00a6:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x00a6
.L_00a8:
    stm r3!, {r4, r5, r6, r7} @ ROM +0x00a8
.L_00aa:
    subs r1, #1 @ ROM +0x00aa
.L_00ac:
    bgt .L_00a2 @ ROM +0x00ac
.L_00ae:
    mov r4, r8 @ ROM +0x00ae
.L_00b0:
    adr r0, .L_0190 @ ROM +0x00b0
.L_00b2:
    str r4, [r0] @ ROM +0x00b2
.L_00b4:
    ldr r4, [sp, #0x18] @ ROM +0x00b4
.L_00b6:
    ldr r0, [r4, #0x18] @ ROM +0x00b6
.L_00b8:
    mov ip, r0 @ ROM +0x00b8
.L_00ba:
    ldrb r0, [r4, #6] @ ROM +0x00ba
.L_00bc:
    adds r4, #0x50 @ ROM +0x00bc
.L_00be:
    str r0, [sp, #4] @ ROM +0x00be
.L_00c0:
    ldr r3, [r4, #0x24] @ ROM +0x00c0
.L_00c2:
    ldrb r6, [r4] @ ROM +0x00c2
.L_00c4:
    movs r0, #0xc7 @ ROM +0x00c4
.L_00c6:
    tst r0, r6 @ ROM +0x00c6
.L_00c8:
    beq .L_0106 @ ROM +0x00c8
.L_00ca:
    lsls r0, r6, #0x19 @ ROM +0x00ca
.L_00cc:
    blo .L_00f4 @ ROM +0x00cc
.L_00ce:
    bmi .L_0102 @ ROM +0x00ce
.L_00d0:
    movs r6, #3 @ ROM +0x00d0
.L_00d2:
    adds r0, r3, #0 @ ROM +0x00d2
.L_00d4:
    adds r0, #0x10 @ ROM +0x00d4
.L_00d6:
    ldr r1, [r4, #0x18] @ ROM +0x00d6
.L_00d8:
    adds r0, r0, r1 @ ROM +0x00d8
.L_00da:
    str r0, [r4, #0x28] @ ROM +0x00da
.L_00dc:
    ldr r0, [r3, #0xc] @ ROM +0x00dc
.L_00de:
    subs r0, r0, r1 @ ROM +0x00de
.L_00e0:
    str r0, [r4, #0x18] @ ROM +0x00e0
.L_00e2:
    movs r5, #0 @ ROM +0x00e2
.L_00e4:
    strb r5, [r4, #9] @ ROM +0x00e4
.L_00e6:
    str r5, [r4, #0x1c] @ ROM +0x00e6
.L_00e8:
    ldrb r2, [r3, #3] @ ROM +0x00e8
.L_00ea:
    lsrs r0, r2, #6 @ ROM +0x00ea
.L_00ec:
    beq .L_0144 @ ROM +0x00ec
.L_00ee:
    movs r0, #0x10 @ ROM +0x00ee
.L_00f0:
    orrs r6, r0 @ ROM +0x00f0
.L_00f2:
    b .L_0144 @ ROM +0x00f2
.L_00f4:
    ldrb r5, [r4, #9] @ ROM +0x00f4
.L_00f6:
    lsls r0, r6, #0x1d @ ROM +0x00f6
.L_00f8:
    bpl .L_0108 @ ROM +0x00f8
.L_00fa:
    ldrb r0, [r4, #0xd] @ ROM +0x00fa
.L_00fc:
    subs r0, #1 @ ROM +0x00fc
.L_00fe:
    strb r0, [r4, #0xd] @ ROM +0x00fe
.L_0100:
    bhi .L_0152 @ ROM +0x0100
.L_0102:
    movs r0, #0 @ ROM +0x0102
.L_0104:
    strb r0, [r4] @ ROM +0x0104
.L_0106:
    b .L_061c @ ROM +0x0106
.L_0108:
    lsls r0, r6, #0x19 @ ROM +0x0108
.L_010a:
    bpl .L_0126 @ ROM +0x010a
.L_010c:
    ldrb r0, [r4, #7] @ ROM +0x010c
.L_010e:
    muls r5, r0, r5 @ ROM +0x010e
.L_0110:
    lsrs r5, r5, #8 @ ROM +0x0110
.L_0112:
    beq .L_011a @ ROM +0x0112
.L_0114:
    ldrb r0, [r4, #0xc] @ ROM +0x0114
.L_0116:
    cmp r5, r0 @ ROM +0x0116
.L_0118:
    bhi .L_0152 @ ROM +0x0118
.L_011a:
    ldrb r5, [r4, #0xc] @ ROM +0x011a
.L_011c:
    cmp r5, #0 @ ROM +0x011c
.L_011e:
    beq .L_0102 @ ROM +0x011e
.L_0120:
    movs r0, #4 @ ROM +0x0120
.L_0122:
    orrs r6, r0 @ ROM +0x0122
.L_0124:
    b .L_0150 @ ROM +0x0124
.L_0126:
    movs r2, #3 @ ROM +0x0126
.L_0128:
    ands r2, r6 @ ROM +0x0128
.L_012a:
    cmp r2, #2 @ ROM +0x012a
.L_012c:
    bne .L_0140 @ ROM +0x012c
.L_012e:
    ldrb r0, [r4, #5] @ ROM +0x012e
.L_0130:
    muls r5, r0, r5 @ ROM +0x0130
.L_0132:
    lsrs r5, r5, #8 @ ROM +0x0132
.L_0134:
    ldrb r0, [r4, #6] @ ROM +0x0134
.L_0136:
    cmp r5, r0 @ ROM +0x0136
.L_0138:
    bhi .L_0152 @ ROM +0x0138
.L_013a:
    adds r5, r0, #0 @ ROM +0x013a
.L_013c:
    beq .L_011a @ ROM +0x013c
.L_013e:
    b .L_014e @ ROM +0x013e
.L_0140:
    cmp r2, #3 @ ROM +0x0140
.L_0142:
    bne .L_0152 @ ROM +0x0142
.L_0144:
    ldrb r0, [r4, #4] @ ROM +0x0144
.L_0146:
    adds r5, r5, r0 @ ROM +0x0146
.L_0148:
    cmp r5, #0xff @ ROM +0x0148
.L_014a:
    blo .L_0150 @ ROM +0x014a
.L_014c:
    movs r5, #0xff @ ROM +0x014c
.L_014e:
    subs r6, #1 @ ROM +0x014e
.L_0150:
    strb r6, [r4] @ ROM +0x0150
.L_0152:
    strb r5, [r4, #9] @ ROM +0x0152
.L_0154:
    ldr r0, [sp, #0x18] @ ROM +0x0154
.L_0156:
    ldrb r0, [r0, #7] @ ROM +0x0156
.L_0158:
    adds r0, #1 @ ROM +0x0158
.L_015a:
    muls r5, r0, r5 @ ROM +0x015a
.L_015c:
    ldrb r0, [r4, #3] @ ROM +0x015c
.L_015e:
    muls r0, r5, r0 @ ROM +0x015e
.L_0160:
    lsrs r0, r0, #0xd @ ROM +0x0160
.L_0162:
    mov sl, r0 @ ROM +0x0162
.L_0164:
    ldrb r0, [r4, #2] @ ROM +0x0164
.L_0166:
    muls r0, r5, r0 @ ROM +0x0166
.L_0168:
    lsrs r0, r0, #0xd @ ROM +0x0168
.L_016a:
    mov fp, r0 @ ROM +0x016a
.L_016c:
    movs r0, #0x10 @ ROM +0x016c
.L_016e:
    ands r0, r6 @ ROM +0x016e
.L_0170:
    beq .L_017c @ ROM +0x0170
.L_0172:
    adds r3, #8 @ ROM +0x0172
.L_0174:
    ldm r3!, {r0, r1} @ ROM +0x0174
.L_0176:
    adds r3, r0, r3 @ ROM +0x0176
.L_0178:
    str r3, [sp, #0xc] @ ROM +0x0178
.L_017a:
    subs r0, r1, r0 @ ROM +0x017a
.L_017c:
    str r0, [sp, #0x10] @ ROM +0x017c
.L_017e:
    ldr r5, [pc, #0xc] @ ROM +0x017e
.L_0180:
    ldr r2, [r4, #0x18] @ ROM +0x0180
.L_0182:
    ldr r3, [r4, #0x28] @ ROM +0x0182
.L_0184:
    ldrb r0, [r4, #1] @ ROM +0x0184
.L_0186:
    adr r1, .L_0194 @ ROM +0x0186
.L_0188:
    bx r1 @ ROM +0x0188
.L_018a:
    mov r8, r8 @ ROM +0x018a
.L_018c:
    .word 0x03002918 @ PC-relative literal
.L_0190:
    .word 0xffffffff @ PC-relative literal
.L_0194:
.arm
    ldr r8, [pc, #-0xc] @ ROM +0x0194
.L_0198:
    orrs fp, sl, fp, lsl #16 @ ROM +0x0198
.L_019c:
    beq .L_0614 @ ROM +0x019c
.L_01a0:
    tst r0, #8 @ ROM +0x01a0
.L_01a4:
    bne .L_053c @ ROM +0x01a4
.L_01a8:
    tst r0, #0x30 @ ROM +0x01a8
.L_01ac:
    bne .L_0894 @ ROM +0x01ac
.L_01b0:
    push {r4, sb, ip} @ ROM +0x01b0
.L_01b4:
    movs r2, r2 @ ROM +0x01b4
.L_01b8:
    orreq r0, r0, #0x40 @ ROM +0x01b8
.L_01bc:
    strbeq r0, [r4, #1] @ ROM +0x01bc
.L_01c0:
    add r4, r4, #0x1c @ ROM +0x01c0
.L_01c4:
    ldm r4, {r7, lr} @ ROM +0x01c4
.L_01c8:
    mul r4, ip, lr @ ROM +0x01c8
.L_01cc:
    ldrsb r6, [r3], #1 @ ROM +0x01cc
.L_01d0:
    ldrsb ip, [r3] @ ROM +0x01d0
.L_01d4:
    tst r0, #0x40 @ ROM +0x01d4
.L_01d8:
    bne .L_06e0 @ ROM +0x01d8
.L_01dc:
    sub ip, ip, r6 @ ROM +0x01dc
.L_01e0:
    lsrs fp, fp, #1 @ ROM +0x01e0
.L_01e4:
    adc fp, fp, #0x8000 @ ROM +0x01e4
.L_01e8:
    bic fp, fp, #0xff00 @ ROM +0x01e8
.L_01ec:
    mov r1, r7 @ ROM +0x01ec
.L_01f0:
    umlal r1, r0, r4, r8 @ ROM +0x01f0
.L_01f4:
    lsr r1, r1, #0x17 @ ROM +0x01f4
.L_01f8:
    orr r0, r1, r0, lsl #9 @ ROM +0x01f8
.L_01fc:
    cmp r2, r0 @ ROM +0x01fc
.L_0200:
    ble .L_0464 @ ROM +0x0200
.L_0204:
    sub r2, r2, r0 @ ROM +0x0204
.L_0208:
    ldr sl, [pc, #0x204] @ ROM +0x0208
.L_020c:
    add sl, sl, r0 @ ROM +0x020c
.L_0210:
    cmp sl, sp @ ROM +0x0210
.L_0214:
    add sl, r3, r0 @ ROM +0x0214
.L_0218:
    add sb, pc, #0x1e8 @ ROM +0x0218
.L_021c:
    stm sb, {r2, sl, sp} @ ROM +0x021c
.L_0220:
    cmplo r0, #0x400 @ ROM +0x0220
.L_0224:
    bhs .L_0260 @ ROM +0x0224
.L_0228:
    bic r1, r3, #3 @ ROM +0x0228
.L_022c:
    mov sb, #0x4000000 @ ROM +0x022c
.L_0230:
    add sb, sb, #0xd4 @ ROM +0x0230
.L_0234:
    add r0, r0, #7 @ ROM +0x0234
.L_0238:
    lsr r0, r0, #2 @ ROM +0x0238
.L_023c:
    sub sp, sp, r0, lsl #2 @ ROM +0x023c
.L_0240:
    and r3, r3, #3 @ ROM +0x0240
.L_0244:
    add r3, r3, sp @ ROM +0x0244
.L_0248:
    orr lr, r0, #0x84000000 @ ROM +0x0248
.L_024c:
    stm sb, {r1, sp, lr} @ ROM +0x024c
.L_0250:
    mov r0, #0 @ ROM +0x0250
.L_0254:
    mov r1, r0 @ ROM +0x0254
.L_0258:
    mov r2, r1 @ ROM +0x0258
.L_025c:
    stm sb, {r0, r1, r2} @ ROM +0x025c
.L_0260:
    subs r4, r4, #0x800000 @ ROM +0x0260
.L_0264:
    lslpl fp, fp, #1 @ ROM +0x0264
.L_0268:
    add r0, pc, #0x1ac @ ROM +0x0268
.L_026c:
    addpl r0, r0, #0x18 @ ROM +0x026c
.L_0270:
    subspl r4, r4, #0x800000 @ ROM +0x0270
.L_0274:
    addpl r0, r0, #0x18 @ ROM +0x0274
.L_0278:
    addpl r4, r4, #0x800000 @ ROM +0x0278
.L_027c:
    ldr r2, [pc, #0x194] @ ROM +0x027c
.L_0280:
    cmp r0, r2 @ ROM +0x0280
.L_0284:
    beq .L_02c0 @ ROM +0x0284
.L_0288:
    str r0, [pc, #0x188] @ ROM +0x0288
.L_028c:
    ldm r0, {r0, r1, r2, r8, sb, sl} @ ROM +0x028c
.L_0290:
    add lr, pc, #0x34 @ ROM +0x0290
.L_0294:
    stm lr, {r0, r1} @ ROM +0x0294
.L_0298:
    add lr, lr, #0x98 @ ROM +0x0298
.L_029c:
    stm lr, {r0, r1} @ ROM +0x029c
.L_02a0:
    sub lr, lr, #0x8c @ ROM +0x02a0
.L_02a4:
    stm lr, {r2, r8, sb, sl} @ ROM +0x02a4
.L_02a8:
    add lr, lr, #0x98 @ ROM +0x02a8
.L_02ac:
    stm lr, {r2, r8, sb, sl} @ ROM +0x02ac
.L_02b0:
    sub lr, lr, #0x80 @ ROM +0x02b0
.L_02b4:
    adds r5, r5, #0x40000000 @ ROM +0x02b4
.L_02b8:
    blo .L_0294 @ ROM +0x02b8
.L_02bc:
    ldr r8, [pc, #-0x134] @ ROM +0x02bc
.L_02c0:
    mov r2, #0xff000000 @ ROM +0x02c0
.L_02c4:
    ldm r5, {r0, r1, sl, lr} @ ROM +0x02c4
.L_02c8:
    mul sb, r7, ip @ ROM +0x02c8
.L_02cc:
    mov r0, r0 @ ROM +0x02cc
.L_02d0:
    mov r0, r0 @ ROM +0x02d0
.L_02d4:
    mlane r0, fp, sb, r0 @ ROM +0x02d4
.L_02d8:
    mov r0, r0 @ ROM +0x02d8
.L_02dc:
    mov r0, r0 @ ROM +0x02dc
.L_02e0:
    mov r0, r0 @ ROM +0x02e0
.L_02e4:
    mov r0, r0 @ ROM +0x02e4
.L_02e8:
    bic r7, r7, r2, asr #1 @ ROM +0x02e8
.L_02ec:
    mulne sb, r7, ip @ ROM +0x02ec
.L_02f0:
    mov r0, r0 @ ROM +0x02f0
.L_02f4:
    mov r0, r0 @ ROM +0x02f4
.L_02f8:
    mlane r1, fp, sb, r1 @ ROM +0x02f8
.L_02fc:
    mov r0, r0 @ ROM +0x02fc
.L_0300:
    mov r0, r0 @ ROM +0x0300
.L_0304:
    mov r0, r0 @ ROM +0x0304
.L_0308:
    mov r0, r0 @ ROM +0x0308
.L_030c:
    bic r7, r7, r2, asr #1 @ ROM +0x030c
.L_0310:
    mulne sb, r7, ip @ ROM +0x0310
.L_0314:
    mov r0, r0 @ ROM +0x0314
.L_0318:
    mov r0, r0 @ ROM +0x0318
.L_031c:
    mlane sl, fp, sb, sl @ ROM +0x031c
.L_0320:
    mov r0, r0 @ ROM +0x0320
.L_0324:
    mov r0, r0 @ ROM +0x0324
.L_0328:
    mov r0, r0 @ ROM +0x0328
.L_032c:
    mov r0, r0 @ ROM +0x032c
.L_0330:
    bic r7, r7, r2, asr #1 @ ROM +0x0330
.L_0334:
    mulne sb, r7, ip @ ROM +0x0334
.L_0338:
    mov r0, r0 @ ROM +0x0338
.L_033c:
    mov r0, r0 @ ROM +0x033c
.L_0340:
    mlane lr, fp, sb, lr @ ROM +0x0340
.L_0344:
    mov r0, r0 @ ROM +0x0344
.L_0348:
    mov r0, r0 @ ROM +0x0348
.L_034c:
    mov r0, r0 @ ROM +0x034c
.L_0350:
    mov r0, r0 @ ROM +0x0350
.L_0354:
    bic r7, r7, r2, asr #1 @ ROM +0x0354
.L_0358:
    stm r5!, {r0, r1, sl, lr} @ ROM +0x0358
.L_035c:
    ldm r5, {r0, r1, sl, lr} @ ROM +0x035c
.L_0360:
    mulne sb, r7, ip @ ROM +0x0360
.L_0364:
    mov r0, r0 @ ROM +0x0364
.L_0368:
    mov r0, r0 @ ROM +0x0368
.L_036c:
    mlane r0, fp, sb, r0 @ ROM +0x036c
.L_0370:
    mov r0, r0 @ ROM +0x0370
.L_0374:
    mov r0, r0 @ ROM +0x0374
.L_0378:
    mov r0, r0 @ ROM +0x0378
.L_037c:
    mov r0, r0 @ ROM +0x037c
.L_0380:
    bic r7, r7, r2, asr #1 @ ROM +0x0380
.L_0384:
    mulne sb, r7, ip @ ROM +0x0384
.L_0388:
    mov r0, r0 @ ROM +0x0388
.L_038c:
    mov r0, r0 @ ROM +0x038c
.L_0390:
    mlane r1, fp, sb, r1 @ ROM +0x0390
.L_0394:
    mov r0, r0 @ ROM +0x0394
.L_0398:
    mov r0, r0 @ ROM +0x0398
.L_039c:
    mov r0, r0 @ ROM +0x039c
.L_03a0:
    mov r0, r0 @ ROM +0x03a0
.L_03a4:
    bic r7, r7, r2, asr #1 @ ROM +0x03a4
.L_03a8:
    mulne sb, r7, ip @ ROM +0x03a8
.L_03ac:
    mov r0, r0 @ ROM +0x03ac
.L_03b0:
    mov r0, r0 @ ROM +0x03b0
.L_03b4:
    mlane sl, fp, sb, sl @ ROM +0x03b4
.L_03b8:
    mov r0, r0 @ ROM +0x03b8
.L_03bc:
    mov r0, r0 @ ROM +0x03bc
.L_03c0:
    mov r0, r0 @ ROM +0x03c0
.L_03c4:
    mov r0, r0 @ ROM +0x03c4
.L_03c8:
    bic r7, r7, r2, asr #1 @ ROM +0x03c8
.L_03cc:
    mulne sb, r7, ip @ ROM +0x03cc
.L_03d0:
    mov r0, r0 @ ROM +0x03d0
.L_03d4:
    mov r0, r0 @ ROM +0x03d4
.L_03d8:
    mlane lr, fp, sb, lr @ ROM +0x03d8
.L_03dc:
    mov r0, r0 @ ROM +0x03dc
.L_03e0:
    mov r0, r0 @ ROM +0x03e0
.L_03e4:
    mov r0, r0 @ ROM +0x03e4
.L_03e8:
    mov r0, r0 @ ROM +0x03e8
.L_03ec:
    bic r7, r7, r2, asr #1 @ ROM +0x03ec
.L_03f0:
    stm r5!, {r0, r1, sl, lr} @ ROM +0x03f0
.L_03f4:
    subs r8, r8, #8 @ ROM +0x03f4
.L_03f8:
    bgt .L_02c4 @ ROM +0x03f8
.L_03fc:
    add ip, pc, #4 @ ROM +0x03fc
.L_0400:
    ldm ip, {r2, r3, sp} @ ROM +0x0400
.L_0404:
    b .L_04b4 @ ROM +0x0404
.L_0408:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0409:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_040a:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_040b:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_040c:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_040d:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_040e:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_040f:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0410:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0411:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0412:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0413:
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0414:
    .word 0x03007910 @ PC-relative literal
.L_0418:
    .word 0x00000000 @ PC-relative literal
.L_041c:
    @ Unrolled ARM DSP fragment entered through computed-PC dispatch.
    asr sb, sb, #0x16 @ ROM +0x041c
.L_0420:
    adds sb, sb, r6, lsl #1 @ ROM +0x0420
.L_0424:
    adds r7, r7, r4 @ ROM +0x0424
.L_0428:
    addpl r6, ip, r6 @ ROM +0x0428
.L_042c:
    ldrsbpl ip, [r3, #1]! @ ROM +0x042c
.L_0430:
    subspl ip, ip, r6 @ ROM +0x0430
.L_0434:
    adds sb, r6, sb, asr #23 @ ROM +0x0434
.L_0438:
    add r6, ip, r6 @ ROM +0x0438
.L_043c:
    adds r7, r7, r4 @ ROM +0x043c
.L_0440:
    ldrsbpl r6, [r3, #1]! @ ROM +0x0440
.L_0444:
    ldrsb ip, [r3, #1]! @ ROM +0x0444
.L_0448:
    subs ip, ip, r6 @ ROM +0x0448
.L_044c:
    adds sb, r6, sb, asr #23 @ ROM +0x044c
.L_0450:
    add r7, r7, r4 @ ROM +0x0450
.L_0454:
    add r3, r3, r7, lsr #23 @ ROM +0x0454
.L_0458:
    ldrsb r6, [r3] @ ROM +0x0458
.L_045c:
    ldrsb ip, [r3, #1]! @ ROM +0x045c
.L_0460:
    subs ip, ip, r6 @ ROM +0x0460
.L_0464:
    add r5, r5, r8, lsl #2 @ ROM +0x0464
.L_0468:
    mul sb, r7, ip @ ROM +0x0468
.L_046c:
    asr sb, sb, #0x16 @ ROM +0x046c
.L_0470:
    adds sb, sb, r6, lsl #1 @ ROM +0x0470
.L_0474:
    ldrne r0, [r5, -r8, lsl #2] @ ROM +0x0474
.L_0478:
    mlane r0, fp, sb, r0 @ ROM +0x0478
.L_047c:
    strne r0, [r5, -r8, lsl #2] @ ROM +0x047c
.L_0480:
    add r7, r7, r4 @ ROM +0x0480
.L_0484:
    lsrs sb, r7, #0x17 @ ROM +0x0484
.L_0488:
    beq .L_04ac @ ROM +0x0488
.L_048c:
    subs r2, r2, r7, lsr #23 @ ROM +0x048c
.L_0490:
    blle .L_04c4 @ ROM +0x0490
.L_0494:
    subs sb, sb, #1 @ ROM +0x0494
.L_0498:
    addeq r6, ip, r6 @ ROM +0x0498
.L_049c:
    ldrsbne r6, [r3, sb]! @ ROM +0x049c
.L_04a0:
    ldrsb ip, [r3, #1]! @ ROM +0x04a0
.L_04a4:
    sub ip, ip, r6 @ ROM +0x04a4
.L_04a8:
    bic r7, r7, #0x3f800000 @ ROM +0x04a8
.L_04ac:
    subs r8, r8, #1 @ ROM +0x04ac
.L_04b0:
    bgt .L_0468 @ ROM +0x04b0
.L_04b4:
    sub r3, r3, #1 @ ROM +0x04b4
.L_04b8:
    pop {r4, sb, ip} @ ROM +0x04b8
.L_04bc:
    str r7, [r4, #0x1c] @ ROM +0x04bc
.L_04c0:
    b .L_060c @ ROM +0x04c0
.L_04c4:
    add r3, sp, #0x18 @ ROM +0x04c4
.L_04c8:
    ldm r3, {r3, r6} @ ROM +0x04c8
.L_04cc:
    cmp r6, #0 @ ROM +0x04cc
.L_04d0:
    rsbne sb, r2, #0 @ ROM +0x04d0
.L_04d4:
    addne r2, r6, r2 @ ROM +0x04d4
.L_04d8:
    addne pc, lr, #8 @ ROM +0x04d8
.L_04dc:
    pop {r4, sb, ip} @ ROM +0x04dc
.L_04e0:
    b .L_04f8 @ ROM +0x04e0
.L_04e4:
    ldr r2, [sp, #0x18] @ ROM +0x04e4
.L_04e8:
    movs r6, r2 @ ROM +0x04e8
.L_04ec:
    ldrne r3, [sp, #0x14] @ ROM +0x04ec
.L_04f0:
    bxne lr @ ROM +0x04f0
.L_04f4:
    pop {r4, sb} @ ROM +0x04f4
.L_04f8:
    strb r6, [r4] @ ROM +0x04f8
.L_04fc:
    b .L_0614 @ ROM +0x04fc
.L_0500:
    @ Unrolled ARM DSP fragment entered through computed-PC dispatch.
    lsls r6, sl, #0x18 @ ROM +0x0500
.L_0504:
    asrs r6, r6, #0x18 @ ROM +0x0504
.L_0508:
    lsls r6, sl, #0x10 @ ROM +0x0508
.L_050c:
    asrs r6, r6, #0x18 @ ROM +0x050c
.L_0510:
    lsls r6, sl, #8 @ ROM +0x0510
.L_0514:
    asrs r6, r6, #0x18 @ ROM +0x0514
.L_0518:
    asrs r6, sl, #0x18 @ ROM +0x0518
.L_051c:
    ldm r3!, {sl} @ ROM +0x051c
.L_0520:
    lsls r6, sl, #0x18 @ ROM +0x0520
.L_0524:
    asrs r6, r6, #0x18 @ ROM +0x0524
.L_0528:
    lsls r6, sl, #0x10 @ ROM +0x0528
.L_052c:
    asrs r6, r6, #0x18 @ ROM +0x052c
.L_0530:
    lsls r6, sl, #8 @ ROM +0x0530
.L_0534:
    asrs r6, r6, #0x18 @ ROM +0x0534
.L_0538:
    pop {r4, sb, ip} @ ROM +0x0538
.L_053c:
    push {r4, sb} @ ROM +0x053c
.L_0540:
    mov lr, r2 @ ROM +0x0540
.L_0544:
    cmp r2, r8 @ ROM +0x0544
.L_0548:
    movgt lr, r8 @ ROM +0x0548
.L_054c:
    sub lr, lr, #1 @ ROM +0x054c
.L_0550:
    lsrs lr, lr, #2 @ ROM +0x0550
.L_0554:
    beq .L_05dc @ ROM +0x0554
.L_0558:
    sub r8, r8, lr, lsl #2 @ ROM +0x0558
.L_055c:
    sub r2, r2, lr, lsl #2 @ ROM +0x055c
.L_0560:
    add r1, pc, #0x34 @ ROM +0x0560
.L_0564:
    sub r0, pc, #0x6c @ ROM +0x0564
.L_0568:
    lsl sb, r3, #0x1e @ ROM +0x0568
.L_056c:
    add r0, r0, sb, lsr #27 @ ROM +0x056c
.L_0570:
    ldm r0!, {r6, r7, sb, sl} @ ROM +0x0570
.L_0574:
    stm r1, {r6, r7} @ ROM +0x0574
.L_0578:
    add r1, r1, #0xc @ ROM +0x0578
.L_057c:
    stm r1, {sb, sl} @ ROM +0x057c
.L_0580:
    add r1, r1, #0xc @ ROM +0x0580
.L_0584:
    ldm r0, {r6, r7, sb, sl} @ ROM +0x0584
.L_0588:
    stm r1, {r6, r7} @ ROM +0x0588
.L_058c:
    add r1, r1, #0xc @ ROM +0x058c
.L_0590:
    stm r1, {sb, sl} @ ROM +0x0590
.L_0594:
    ldm r3!, {sl} @ ROM +0x0594
.L_0598:
    ldm r5, {r0, r1, r7, sb} @ ROM +0x0598
.L_059c:
    mov r0, r0 @ ROM +0x059c
.L_05a0:
    mov r0, r0 @ ROM +0x05a0
.L_05a4:
    mlane r0, fp, r6, r0 @ ROM +0x05a4
.L_05a8:
    mov r0, r0 @ ROM +0x05a8
.L_05ac:
    mov r0, r0 @ ROM +0x05ac
.L_05b0:
    mlane r1, fp, r6, r1 @ ROM +0x05b0
.L_05b4:
    mov r0, r0 @ ROM +0x05b4
.L_05b8:
    mov r0, r0 @ ROM +0x05b8
.L_05bc:
    mlane r7, fp, r6, r7 @ ROM +0x05bc
.L_05c0:
    mov r0, r0 @ ROM +0x05c0
.L_05c4:
    mov r0, r0 @ ROM +0x05c4
.L_05c8:
    mlane sb, fp, r6, sb @ ROM +0x05c8
.L_05cc:
    stm r5!, {r0, r1, r7, sb} @ ROM +0x05cc
.L_05d0:
    subs lr, lr, #1 @ ROM +0x05d0
.L_05d4:
    bne .L_0598 @ ROM +0x05d4
.L_05d8:
    sub r3, r3, #4 @ ROM +0x05d8
.L_05dc:
    mov r1, #4 @ ROM +0x05dc
.L_05e0:
    ldr r0, [r5] @ ROM +0x05e0
.L_05e4:
    ldrsb r6, [r3], #1 @ ROM +0x05e4
.L_05e8:
    mla r0, fp, r6, r0 @ ROM +0x05e8
.L_05ec:
    str r0, [r5], #4 @ ROM +0x05ec
.L_05f0:
    subs r2, r2, #1 @ ROM +0x05f0
.L_05f4:
    bleq .L_04e4 @ ROM +0x05f4
.L_05f8:
    subs r1, r1, #1 @ ROM +0x05f8
.L_05fc:
    bgt .L_05e0 @ ROM +0x05fc
.L_0600:
    subs r8, r8, #4 @ ROM +0x0600
.L_0604:
    bgt .L_0540 @ ROM +0x0604
.L_0608:
    pop {r4, sb} @ ROM +0x0608
.L_060c:
    str r2, [r4, #0x18] @ ROM +0x060c
.L_0610:
    str r3, [r4, #0x28] @ ROM +0x0610
.L_0614:
    add r0, pc, #1 @ ROM +0x0614
.L_0618:
    bx r0 @ ROM +0x0618
.L_061c:
.thumb
    ldr r0, [sp, #4] @ ROM +0x061c
.L_061e:
    subs r0, #1 @ ROM +0x061e
.L_0620:
    ble .L_0626 @ ROM +0x0620
.L_0622:
    adds r4, #0x40 @ ROM +0x0622
.L_0624:
    b .L_00be @ ROM +0x0624
.L_0626:
    adr r0, .L_0644 @ ROM +0x0626
.L_0628:
    bx r0 @ ROM +0x0628
.L_062a:
    ldr r0, [sp, #0x18] @ ROM +0x062a
.L_062c:
    ldr r3, [pc, #0x10] @ ROM +0x062c
.L_062e:
    str r3, [r0] @ ROM +0x062e
.L_0630:
    add sp, #0x1c @ ROM +0x0630
.L_0632:
    pop {r0, r1, r2, r3, r4, r5, r6, r7} @ ROM +0x0632
.L_0634:
    mov r8, r0 @ ROM +0x0634
.L_0636:
    mov sb, r1 @ ROM +0x0636
.L_0638:
    mov sl, r2 @ ROM +0x0638
.L_063a:
    mov fp, r3 @ ROM +0x063a
.L_063c:
    pop {r3} @ ROM +0x063c
.L_063e:
    bx r3 @ ROM +0x063e
.L_0640:
    .word 0x68736d53 @ PC-relative literal
.L_0644:
.arm
    ldr sl, [pc, #-0x4c0] @ ROM +0x0644
.L_0648:
    ldr sb, [sp, #8] @ ROM +0x0648
.L_064c:
    ldr r8, [pc, #-0x4c4] @ ROM +0x064c
.L_0650:
    mov fp, #0xff @ ROM +0x0650
.L_0654:
    mvn ip, #0 @ ROM +0x0654
.L_0658:
    lsl ip, ip, #0xe @ ROM +0x0658
.L_065c:
    mov r7, #0x630 @ ROM +0x065c
.L_0660:
    ldrsh r2, [sl], #2 @ ROM +0x0660
.L_0664:
    ldrsh r0, [sl], #2 @ ROM +0x0664
.L_0668:
    ldrsh r3, [sl], #2 @ ROM +0x0668
.L_066c:
    ldrsh r1, [sl], #2 @ ROM +0x066c
.L_0670:
    cmp r0, #0x4000 @ ROM +0x0670
.L_0674:
    movge r0, #0x3f80 @ ROM +0x0674
.L_0678:
    cmn r0, #0x4000 @ ROM +0x0678
.L_067c:
    movlt r0, ip @ ROM +0x067c
.L_0680:
    cmp r1, #0x4000 @ ROM +0x0680
.L_0684:
    movge r1, #0x3f80 @ ROM +0x0684
.L_0688:
    cmn r1, #0x4000 @ ROM +0x0688
.L_068c:
    movlt r1, ip @ ROM +0x068c
.L_0690:
    cmp r2, #0x4000 @ ROM +0x0690
.L_0694:
    movge r2, #0x3f80 @ ROM +0x0694
.L_0698:
    cmn r2, #0x4000 @ ROM +0x0698
.L_069c:
    movlt r2, ip @ ROM +0x069c
.L_06a0:
    cmp r3, #0x4000 @ ROM +0x06a0
.L_06a4:
    movge r3, #0x3f80 @ ROM +0x06a4
.L_06a8:
    cmn r3, #0x4000 @ ROM +0x06a8
.L_06ac:
    movlt r3, ip @ ROM +0x06ac
.L_06b0:
    and r0, fp, r0, asr #7 @ ROM +0x06b0
.L_06b4:
    and r1, fp, r1, asr #7 @ ROM +0x06b4
.L_06b8:
    and r2, fp, r2, asr #7 @ ROM +0x06b8
.L_06bc:
    and r3, fp, r3, asr #7 @ ROM +0x06bc
.L_06c0:
    orr r2, r2, r3, lsl #8 @ ROM +0x06c0
.L_06c4:
    orr r0, r0, r1, lsl #8 @ ROM +0x06c4
.L_06c8:
    strh r2, [sb, r7] @ ROM +0x06c8
.L_06cc:
    strh r0, [sb], #2 @ ROM +0x06cc
.L_06d0:
    subs r8, r8, #2 @ ROM +0x06d0
.L_06d4:
    bgt .L_0660 @ ROM +0x06d4
.L_06d8:
    sub r0, pc, #0xb5 @ ROM +0x06d8
.L_06dc:
    bx r0 @ ROM +0x06dc
.L_06e0:
    cmp ip, #0 @ ROM +0x06e0
.L_06e4:
    bne .L_07ac @ ROM +0x06e4
.L_06e8:
    ldrb r6, [r3, #2] @ ROM +0x06e8
.L_06ec:
    add r2, r2, r6, lsl #24 @ ROM +0x06ec
.L_06f0:
    ldrb r6, [r3, #4] @ ROM +0x06f0
.L_06f4:
    adds r6, r2, r6, lsl #24 @ ROM +0x06f4
.L_06f8:
    mvnmi r6, r6 @ ROM +0x06f8
.L_06fc:
    lsr sl, r6, #8 @ ROM +0x06fc
.L_0700:
    ldrb r1, [r3, #3] @ ROM +0x0700
.L_0704:
    ldrb r0, [r3, #1] @ ROM +0x0704
.L_0708:
    lsl r0, r0, #0x18 @ ROM +0x0708
.L_070c:
    mla r6, sl, r1, r0 @ ROM +0x070c
.L_0710:
    push {r2, r3, sb, ip} @ ROM +0x0710
.L_0714:
    ldm r5, {r0, r1, r2, r3, sb, sl, ip, lr} @ ROM +0x0714
.L_0718:
    cmp r7, r6 @ ROM +0x0718
.L_071c:
    addlo r0, r0, fp, lsl #6 @ ROM +0x071c
.L_0720:
    subhs r0, r0, fp, lsl #6 @ ROM +0x0720
.L_0724:
    adds r7, r7, r4, lsl #3 @ ROM +0x0724
.L_0728:
    cmp r7, r6 @ ROM +0x0728
.L_072c:
    addlo r1, r1, fp, lsl #6 @ ROM +0x072c
.L_0730:
    subhs r1, r1, fp, lsl #6 @ ROM +0x0730
.L_0734:
    adds r7, r7, r4, lsl #3 @ ROM +0x0734
.L_0738:
    cmp r7, r6 @ ROM +0x0738
.L_073c:
    addlo r2, r2, fp, lsl #6 @ ROM +0x073c
.L_0740:
    subhs r2, r2, fp, lsl #6 @ ROM +0x0740
.L_0744:
    adds r7, r7, r4, lsl #3 @ ROM +0x0744
.L_0748:
    cmp r7, r6 @ ROM +0x0748
.L_074c:
    addlo r3, r3, fp, lsl #6 @ ROM +0x074c
.L_0750:
    subhs r3, r3, fp, lsl #6 @ ROM +0x0750
.L_0754:
    adds r7, r7, r4, lsl #3 @ ROM +0x0754
.L_0758:
    cmp r7, r6 @ ROM +0x0758
.L_075c:
    addlo sb, sb, fp, lsl #6 @ ROM +0x075c
.L_0760:
    subhs sb, sb, fp, lsl #6 @ ROM +0x0760
.L_0764:
    adds r7, r7, r4, lsl #3 @ ROM +0x0764
.L_0768:
    cmp r7, r6 @ ROM +0x0768
.L_076c:
    addlo sl, sl, fp, lsl #6 @ ROM +0x076c
.L_0770:
    subhs sl, sl, fp, lsl #6 @ ROM +0x0770
.L_0774:
    adds r7, r7, r4, lsl #3 @ ROM +0x0774
.L_0778:
    cmp r7, r6 @ ROM +0x0778
.L_077c:
    addlo ip, ip, fp, lsl #6 @ ROM +0x077c
.L_0780:
    subhs ip, ip, fp, lsl #6 @ ROM +0x0780
.L_0784:
    adds r7, r7, r4, lsl #3 @ ROM +0x0784
.L_0788:
    cmp r7, r6 @ ROM +0x0788
.L_078c:
    addlo lr, lr, fp, lsl #6 @ ROM +0x078c
.L_0790:
    subhs lr, lr, fp, lsl #6 @ ROM +0x0790
.L_0794:
    adds r7, r7, r4, lsl #3 @ ROM +0x0794
.L_0798:
    stm r5!, {r0, r1, r2, r3, sb, sl, ip, lr} @ ROM +0x0798
.L_079c:
    subs r8, r8, #8 @ ROM +0x079c
.L_07a0:
    bgt .L_0714 @ ROM +0x07a0
.L_07a4:
    pop {r2, r3, sb, ip} @ ROM +0x07a4
.L_07a8:
    b .L_04b4 @ ROM +0x07a8
.L_07ac:
    subs ip, ip, #1 @ ROM +0x07ac
.L_07b0:
    bne .L_0838 @ ROM +0x07b0
.L_07b4:
    mov r6, #0x300 @ ROM +0x07b4
.L_07b8:
    lsr fp, fp, #1 @ ROM +0x07b8
.L_07bc:
    bic fp, fp, #0xff00 @ ROM +0x07bc
.L_07c0:
    mov ip, #0x70 @ ROM +0x07c0
.L_07c4:
    ldm r5, {r0, r1, sl, lr} @ ROM +0x07c4
.L_07c8:
    adds r7, r7, r4, lsl #3 @ ROM +0x07c8
.L_07cc:
    rsb sb, ip, r7, lsr #24 @ ROM +0x07cc
.L_07d0:
    lsl r6, r7, #1 @ ROM +0x07d0
.L_07d4:
    sub sb, sb, r6, lsr #27 @ ROM +0x07d4
.L_07d8:
    adds r2, sb, r2, asr #1 @ ROM +0x07d8
.L_07dc:
    mlane r0, fp, r2, r0 @ ROM +0x07dc
.L_07e0:
    adds r7, r7, r4, lsl #3 @ ROM +0x07e0
.L_07e4:
    rsb sb, ip, r7, lsr #24 @ ROM +0x07e4
.L_07e8:
    lsl r6, r7, #1 @ ROM +0x07e8
.L_07ec:
    sub sb, sb, r6, lsr #27 @ ROM +0x07ec
.L_07f0:
    adds r2, sb, r2, asr #1 @ ROM +0x07f0
.L_07f4:
    mlane r1, fp, r2, r1 @ ROM +0x07f4
.L_07f8:
    adds r7, r7, r4, lsl #3 @ ROM +0x07f8
.L_07fc:
    rsb sb, ip, r7, lsr #24 @ ROM +0x07fc
.L_0800:
    lsl r6, r7, #1 @ ROM +0x0800
.L_0804:
    sub sb, sb, r6, lsr #27 @ ROM +0x0804
.L_0808:
    adds r2, sb, r2, asr #1 @ ROM +0x0808
.L_080c:
    mlane sl, fp, r2, sl @ ROM +0x080c
.L_0810:
    adds r7, r7, r4, lsl #3 @ ROM +0x0810
.L_0814:
    rsb sb, ip, r7, lsr #24 @ ROM +0x0814
.L_0818:
    lsl r6, r7, #1 @ ROM +0x0818
.L_081c:
    sub sb, sb, r6, lsr #27 @ ROM +0x081c
.L_0820:
    adds r2, sb, r2, asr #1 @ ROM +0x0820
.L_0824:
    mlane lr, fp, r2, lr @ ROM +0x0824
.L_0828:
    stm r5!, {r0, r1, sl, lr} @ ROM +0x0828
.L_082c:
    subs r8, r8, #4 @ ROM +0x082c
.L_0830:
    bgt .L_07c4 @ ROM +0x0830
.L_0834:
    b .L_04b4 @ ROM +0x0834
.L_0838:
    mov r6, #0x80 @ ROM +0x0838
.L_083c:
    mov ip, #0x180 @ ROM +0x083c
.L_0840:
    ldm r5, {r0, r1, sl, lr} @ ROM +0x0840
.L_0844:
    adds r7, r7, r4, lsl #3 @ ROM +0x0844
.L_0848:
    rsbpl sb, r6, r7, asr #23 @ ROM +0x0848
.L_084c:
    submi sb, ip, r7, lsr #23 @ ROM +0x084c
.L_0850:
    mla r0, fp, sb, r0 @ ROM +0x0850
.L_0854:
    adds r7, r7, r4, lsl #3 @ ROM +0x0854
.L_0858:
    rsbpl sb, r6, r7, asr #23 @ ROM +0x0858
.L_085c:
    submi sb, ip, r7, lsr #23 @ ROM +0x085c
.L_0860:
    mla r1, fp, sb, r1 @ ROM +0x0860
.L_0864:
    adds r7, r7, r4, lsl #3 @ ROM +0x0864
.L_0868:
    rsbpl sb, r6, r7, asr #23 @ ROM +0x0868
.L_086c:
    submi sb, ip, r7, lsr #23 @ ROM +0x086c
.L_0870:
    mla sl, fp, sb, sl @ ROM +0x0870
.L_0874:
    adds r7, r7, r4, lsl #3 @ ROM +0x0874
.L_0878:
    rsbpl sb, r6, r7, asr #23 @ ROM +0x0878
.L_087c:
    submi sb, ip, r7, lsr #23 @ ROM +0x087c
.L_0880:
    mla lr, fp, sb, lr @ ROM +0x0880
.L_0884:
    stm r5!, {r0, r1, sl, lr} @ ROM +0x0884
.L_0888:
    subs r8, r8, #4 @ ROM +0x0888
.L_088c:
    bgt .L_0840 @ ROM +0x088c
.L_0890:
    b .L_04b4 @ ROM +0x0890
.L_0894:
    ldr r6, [r4, #0x24] @ ROM +0x0894
.L_0898:
    ldrb r0, [r4] @ ROM +0x0898
.L_089c:
    tst r0, #0x20 @ ROM +0x089c
.L_08a0:
    bne .L_08e4 @ ROM +0x08a0
.L_08a4:
    orr r0, r0, #0x20 @ ROM +0x08a4
.L_08a8:
    strb r0, [r4] @ ROM +0x08a8
.L_08ac:
    ldrb r0, [r4, #1] @ ROM +0x08ac
.L_08b0:
    tst r0, #0x10 @ ROM +0x08b0
.L_08b4:
    beq .L_08cc @ ROM +0x08b4
.L_08b8:
    ldr r1, [r6, #0xc] @ ROM +0x08b8
.L_08bc:
    add r1, r1, r6, lsl #1 @ ROM +0x08bc
.L_08c0:
    add r1, r1, #0x20 @ ROM +0x08c0
.L_08c4:
    sub r3, r1, r3 @ ROM +0x08c4
.L_08c8:
    str r3, [r4, #0x28] @ ROM +0x08c8
.L_08cc:
    ldrh r0, [r6] @ ROM +0x08cc
.L_08d0:
    cmp r0, #0 @ ROM +0x08d0
.L_08d4:
    beq .L_08e4 @ ROM +0x08d4
.L_08d8:
    sub r3, r3, r6 @ ROM +0x08d8
.L_08dc:
    sub r3, r3, #0x10 @ ROM +0x08dc
.L_08e0:
    str r3, [r4, #0x28] @ ROM +0x08e0
.L_08e4:
    push {r4, sb, ip} @ ROM +0x08e4
.L_08e8:
    lsrs fp, fp, #1 @ ROM +0x08e8
.L_08ec:
    adc fp, fp, #0x8000 @ ROM +0x08ec
.L_08f0:
    bic fp, fp, #0xff00 @ ROM +0x08f0
.L_08f4:
    ldr r7, [r4, #0x1c] @ ROM +0x08f4
.L_08f8:
    ldr r1, [r4, #0x20] @ ROM +0x08f8
.L_08fc:
    ldrb r0, [r4, #1] @ ROM +0x08fc
.L_0900:
    tst r0, #8 @ ROM +0x0900
.L_0904:
    movne r1, #0x800000 @ ROM +0x0904
.L_0908:
    muleq r1, ip, r1 @ ROM +0x0908
.L_090c:
    add r5, r5, r8, lsl #2 @ ROM +0x090c
.L_0910:
    ldrh r0, [r6] @ ROM +0x0910
.L_0914:
    cmp r0, #0 @ ROM +0x0914
.L_0918:
    beq .L_0a20 @ ROM +0x0918
.L_091c:
    mov r0, #0xff000000 @ ROM +0x091c
.L_0920:
    str r0, [r4, #0x3c] @ ROM +0x0920
.L_0924:
    ldrb r0, [r4, #1] @ ROM +0x0924
.L_0928:
    tst r0, #0x10 @ ROM +0x0928
.L_092c:
    bne .L_09a4 @ ROM +0x092c
.L_0930:
    bl .L_0a8c @ ROM +0x0930
.L_0934:
    mov r6, ip @ ROM +0x0934
.L_0938:
    add r3, r3, #1 @ ROM +0x0938
.L_093c:
    bl .L_0a8c @ ROM +0x093c
.L_0940:
    sub ip, ip, r6 @ ROM +0x0940
.L_0944:
    mul sb, r7, ip @ ROM +0x0944
.L_0948:
    asr sb, sb, #0x16 @ ROM +0x0948
.L_094c:
    adds sb, sb, r6, lsl #1 @ ROM +0x094c
.L_0950:
    ldrne r0, [r5, -r8, lsl #2] @ ROM +0x0950
.L_0954:
    mlane r0, fp, sb, r0 @ ROM +0x0954
.L_0958:
    strne r0, [r5, -r8, lsl #2] @ ROM +0x0958
.L_095c:
    add r7, r7, r1 @ ROM +0x095c
.L_0960:
    lsrs sb, r7, #0x17 @ ROM +0x0960
.L_0964:
    beq .L_0998 @ ROM +0x0964
.L_0968:
    subs r2, r2, r7, lsr #23 @ ROM +0x0968
.L_096c:
    blle .L_04c4 @ ROM +0x096c
.L_0970:
    subs sb, sb, #1 @ ROM +0x0970
.L_0974:
    addeq r6, ip, r6 @ ROM +0x0974
.L_0978:
    beq .L_0988 @ ROM +0x0978
.L_097c:
    add r3, r3, sb @ ROM +0x097c
.L_0980:
    bl .L_0a8c @ ROM +0x0980
.L_0984:
    mov r6, ip @ ROM +0x0984
.L_0988:
    add r3, r3, #1 @ ROM +0x0988
.L_098c:
    bl .L_0a8c @ ROM +0x098c
.L_0990:
    sub ip, ip, r6 @ ROM +0x0990
.L_0994:
    bic r7, r7, #0x3f800000 @ ROM +0x0994
.L_0998:
    subs r8, r8, #1 @ ROM +0x0998
.L_099c:
    bgt .L_0944 @ ROM +0x099c
.L_09a0:
    b .L_04b4 @ ROM +0x09a0
.L_09a4:
    sub r3, r3, #1 @ ROM +0x09a4
.L_09a8:
    bl .L_0a8c @ ROM +0x09a8
.L_09ac:
    mov r6, ip @ ROM +0x09ac
.L_09b0:
    sub r3, r3, #1 @ ROM +0x09b0
.L_09b4:
    bl .L_0a8c @ ROM +0x09b4
.L_09b8:
    sub ip, ip, r6 @ ROM +0x09b8
.L_09bc:
    mul sb, r7, ip @ ROM +0x09bc
.L_09c0:
    asr sb, sb, #0x16 @ ROM +0x09c0
.L_09c4:
    adds sb, sb, r6, lsl #1 @ ROM +0x09c4
.L_09c8:
    ldrne r0, [r5, -r8, lsl #2] @ ROM +0x09c8
.L_09cc:
    mlane r0, fp, sb, r0 @ ROM +0x09cc
.L_09d0:
    strne r0, [r5, -r8, lsl #2] @ ROM +0x09d0
.L_09d4:
    add r7, r7, r1 @ ROM +0x09d4
.L_09d8:
    lsrs sb, r7, #0x17 @ ROM +0x09d8
.L_09dc:
    beq .L_0a10 @ ROM +0x09dc
.L_09e0:
    subs r2, r2, r7, lsr #23 @ ROM +0x09e0
.L_09e4:
    blle .L_04c4 @ ROM +0x09e4
.L_09e8:
    subs sb, sb, #1 @ ROM +0x09e8
.L_09ec:
    addeq r6, ip, r6 @ ROM +0x09ec
.L_09f0:
    beq .L_0a00 @ ROM +0x09f0
.L_09f4:
    sub r3, r3, sb @ ROM +0x09f4
.L_09f8:
    bl .L_0a8c @ ROM +0x09f8
.L_09fc:
    mov r6, ip @ ROM +0x09fc
.L_0a00:
    sub r3, r3, #1 @ ROM +0x0a00
.L_0a04:
    bl .L_0a8c @ ROM +0x0a04
.L_0a08:
    sub ip, ip, r6 @ ROM +0x0a08
.L_0a0c:
    bic r7, r7, #0x3f800000 @ ROM +0x0a0c
.L_0a10:
    subs r8, r8, #1 @ ROM +0x0a10
.L_0a14:
    bgt .L_09bc @ ROM +0x0a14
.L_0a18:
    add r3, r3, #3 @ ROM +0x0a18
.L_0a1c:
    b .L_04b4 @ ROM +0x0a1c
.L_0a20:
    ldrb r0, [r4, #1] @ ROM +0x0a20
.L_0a24:
    tst r0, #0x10 @ ROM +0x0a24
.L_0a28:
    beq .L_04b4 @ ROM +0x0a28
.L_0a2c:
    ldrsb r6, [r3, #-1]! @ ROM +0x0a2c
.L_0a30:
    ldrsb ip, [r3, #-1] @ ROM +0x0a30
.L_0a34:
    sub ip, ip, r6 @ ROM +0x0a34
.L_0a38:
    mul sb, r7, ip @ ROM +0x0a38
.L_0a3c:
    asr sb, sb, #0x16 @ ROM +0x0a3c
.L_0a40:
    adds sb, sb, r6, lsl #1 @ ROM +0x0a40
.L_0a44:
    ldrne r0, [r5, -r8, lsl #2] @ ROM +0x0a44
.L_0a48:
    mlane r0, fp, sb, r0 @ ROM +0x0a48
.L_0a4c:
    strne r0, [r5, -r8, lsl #2] @ ROM +0x0a4c
.L_0a50:
    add r7, r7, r1 @ ROM +0x0a50
.L_0a54:
    lsrs sb, r7, #0x17 @ ROM +0x0a54
.L_0a58:
    beq .L_0a7c @ ROM +0x0a58
.L_0a5c:
    subs r2, r2, r7, lsr #23 @ ROM +0x0a5c
.L_0a60:
    blle .L_04c4 @ ROM +0x0a60
.L_0a64:
    movs sb, sb @ ROM +0x0a64
.L_0a68:
    addeq r6, ip, r6 @ ROM +0x0a68
.L_0a6c:
    ldrsbne r6, [r3, -sb]! @ ROM +0x0a6c
.L_0a70:
    ldrsb ip, [r3, #-1] @ ROM +0x0a70
.L_0a74:
    sub ip, ip, r6 @ ROM +0x0a74
.L_0a78:
    bic r7, r7, #0x3f800000 @ ROM +0x0a78
.L_0a7c:
    subs r8, r8, #1 @ ROM +0x0a7c
.L_0a80:
    bgt .L_0a38 @ ROM +0x0a80
.L_0a84:
    add r3, r3, #2 @ ROM +0x0a84
.L_0a88:
    b .L_04b4 @ ROM +0x0a88
.L_0a8c:
    push {r0, r2, r5, r6, r7, lr} @ ROM +0x0a8c
.L_0a90:
    lsr r0, r3, #6 @ ROM +0x0a90
.L_0a94:
    ldr ip, [r4, #0x3c] @ ROM +0x0a94
.L_0a98:
    cmp r0, ip @ ROM +0x0a98
.L_0a9c:
    beq .L_0b00 @ ROM +0x0a9c
.L_0aa0:
    str r0, [r4, #0x3c] @ ROM +0x0aa0
.L_0aa4:
    mov ip, #0x21 @ ROM +0x0aa4
.L_0aa8:
    mul r2, ip, r0 @ ROM +0x0aa8
.L_0aac:
    ldr ip, [r4, #0x24] @ ROM +0x0aac
.L_0ab0:
    add r2, r2, ip @ ROM +0x0ab0
.L_0ab4:
    add r2, r2, #0x10 @ ROM +0x0ab4
.L_0ab8:
    ldr r5, [pc, #0x50] @ ROM +0x0ab8
.L_0abc:
    add r6, pc, #0x50 @ ROM +0x0abc
.L_0ac0:
    mov r7, #0x40 @ ROM +0x0ac0
.L_0ac4:
    ldrb lr, [r2], #1 @ ROM +0x0ac4
.L_0ac8:
    strb lr, [r5], #1 @ ROM +0x0ac8
.L_0acc:
    ldrb ip, [r2], #1 @ ROM +0x0acc
.L_0ad0:
    b .L_0ae8 @ ROM +0x0ad0
.L_0ad4:
    ldrb ip, [r2], #1 @ ROM +0x0ad4
.L_0ad8:
    lsr r0, ip, #4 @ ROM +0x0ad8
.L_0adc:
    ldrsb r0, [r6, r0] @ ROM +0x0adc
.L_0ae0:
    add lr, lr, r0 @ ROM +0x0ae0
.L_0ae4:
    strb lr, [r5], #1 @ ROM +0x0ae4
.L_0ae8:
    and r0, ip, #0xf @ ROM +0x0ae8
.L_0aec:
    ldrsb r0, [r6, r0] @ ROM +0x0aec
.L_0af0:
    add lr, lr, r0 @ ROM +0x0af0
.L_0af4:
    strb lr, [r5], #1 @ ROM +0x0af4
.L_0af8:
    subs r7, r7, #2 @ ROM +0x0af8
.L_0afc:
    bgt .L_0ad4 @ ROM +0x0afc
.L_0b00:
    ldr r5, [pc, #8] @ ROM +0x0b00
.L_0b04:
    and r0, r3, #0x3f @ ROM +0x0b04
.L_0b08:
    ldrsb ip, [r5, r0] @ ROM +0x0b08
.L_0b0c:
    pop {r0, r2, r5, r6, r7, pc} @ ROM +0x0b0c
.L_0b10:
    .word 0x03002098 @ PC-relative literal
.L_0b14:
    @ Sixteen signed-byte delta values used by compressed sample decoding.
    .byte 0x00 @ preserved zero storage / decoder lookup data
.L_0b15:
    .byte 0x01 @ preserved zero storage / decoder lookup data
.L_0b16:
    .byte 0x04 @ preserved zero storage / decoder lookup data
.L_0b17:
    .byte 0x09 @ preserved zero storage / decoder lookup data
.L_0b18:
    .byte 0x10 @ preserved zero storage / decoder lookup data
.L_0b19:
    .byte 0x19 @ preserved zero storage / decoder lookup data
.L_0b1a:
    .byte 0x24 @ preserved zero storage / decoder lookup data
.L_0b1b:
    .byte 0x31 @ preserved zero storage / decoder lookup data
.L_0b1c:
    .byte 0xc0 @ preserved zero storage / decoder lookup data
.L_0b1d:
    .byte 0xcf @ preserved zero storage / decoder lookup data
.L_0b1e:
    .byte 0xdc @ preserved zero storage / decoder lookup data
.L_0b1f:
    .byte 0xe7 @ preserved zero storage / decoder lookup data
.L_0b20:
    .byte 0xf0 @ preserved zero storage / decoder lookup data
.L_0b21:
    .byte 0xf7 @ preserved zero storage / decoder lookup data
.L_0b22:
    .byte 0xfc @ preserved zero storage / decoder lookup data
.L_0b23:
    .byte 0xff @ preserved zero storage / decoder lookup data
.global hq_mixer_end
hq_mixer_end:
