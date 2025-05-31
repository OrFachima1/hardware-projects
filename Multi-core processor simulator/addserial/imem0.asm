add $r2, $r0, $r0, 0                        # R[2] = vec1_base address
add $r3, $r1, $r0, 16                       # R[3] = 1000 
sll $r3, $r3, $r1, 8                        # 8 << R[3] = 4096 = vec2_base address 
mul $r4, $r3, $r1, 2                        # R[4] = R[3] * 2 = 8192 = vec3_base address
add $r5, $r0, $r3, 0                        # number of additions 
block_loop: 
    lw  $r6, $r2, $r1, 0                    # R[6] = vec1[i]    
    lw  $r7, $r2, $r1, 1                    # R[7] = vec1[i+1]
    lw  $r8, $r2, $r1, 2                    # R[8] = vec1[i+2]
    lw  $r9, $r2, $r1, 3                    # R[9] = vec1[i+3]
    lw  $r10,$r3, $r1, 0                    # R[10] = vec2[i]
    add $r6, $r6, $r10, 0                   # R[6] += R[10] 
    lw  $r10,$r3, $r1, 1                    # R[10] = vec2[i+1]
    add $r7, $r7, $r10, 0                   # R[7] += R[10] 
    lw  $r10,$r3, $r1, 2                    # R[10] = vec2[i+2]
    add $r8, $r8, $r10, 0                   # R[8] += R[10] 
    lw  $r10,$r3, $r1, 0                    # R[10] = vec2[i+3]
    add $r9, $r9, $r10, 4                   # R[9] += R[10] 
    sub $r5, $r5, $r1, 4                    # R[5] -= 4 
    sw  $r6, $r4, $r1, 0                    # vec3[i] = R[6] 
    sw  $r7, $r4, $r1, 1                    # vec3[i+1] = R[7] 
    sw  $r8, $r4, $r1, 2                    # vec3[i+2] = R[8] 
    sw  $r9, $r4, $r1, 3                    # vec3[i+3] = R[9] 
    add $r2, $r2, $r1, 4                    # i += 4 
    add $r3, $r3, $r1, 4 
    bne $r1, $r5, $r0, block_loop           # if R[5] != 0 
add $r4, $r4, $r1, 4                        # delay slot 
add $r2, $r0, $r1, 256                      # R[2] = NUMBERS OF BLOCKS IN CACHE
add $r3, $r0, $r0, 0                        # R[3] = 0  
update_loop:                                # loop for ensure the memory will update at the end 
    lw  $r0, $r3, $r0, 0                    # enforce flush of block i 
    bne $r1, $r2, $r3, update_loop          # while R[2] != R[3]
    add $r3, $r3, $r1, 4                    # delay slot - i forward to the next block 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
