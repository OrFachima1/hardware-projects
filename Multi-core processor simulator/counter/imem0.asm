add $r2, $r0, $r0, 0                        # Core ID
add $r3, $r1, $r0, 128                      # Amount of increments per core 
add $r4, $r0, $r1, 3                        # Intial mask 8'b11 
wait_for_core_turn:
    lw  $r5, $r0, $r0, 0                    # load counter from memory 
    and $r6, $r4, $r5, 0                    #  R[6] = R[4] & R[5] 
    bne $r1, $r6, $r2, wait_for_core_turn   # it's not our turn, keep waiting 
add $r5, $r5, $r1, 1                        # counter++ 
sw $r5, $r0, $r0, 0                         # update counter 
sub $r3, $r3, $r1, 1                        #  Amount of increments per core-- 
bne $r1, $r3, $r0, wait_for_core_turn       # we didn't finish yet 
nop $r0, $r0, $r0, 0                        # dealy slot 
lw $r0, $r1, $r0, 1024                      # force conplict miss
halt $r0, $r0, $r0, 0 
halt $r0, $r0, $r0, 0 
halt $r0, $r0, $r0, 0 
halt $r0, $r0, $r0, 0 

