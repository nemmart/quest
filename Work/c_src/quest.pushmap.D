# M4b caller map — Project 19 tranche D: 5 RETURN_MESSAGE sites (all
# LCALL; no new code). 70169B82 is the pass-by-reference site: its
# WPSH 0,2 pushes three POINTER values to stack temporaries — the
# pointers are redirected, the temps stay on the stack. RETURN_MESSAGE
# never WRTNs (ends in SYSCALL 0310, retired) — expect a write-mode
# WSAVS/WRTN imbalance of +1 per firing leg.
# RETURN_MESSAGE,6 @ 7015BE74
push 7015BE72 740075CA   # XPEF arg1
push 7015BE6F 740075C8   # LPEF arg2
push 7015BE6D 740075C6   # XPEF arg3
push 7015BE6B 740075C2 2   # WPSH args4-5
push 7015BE69 740075C0   # XPEF arg6
call 7015BE74 740075CC   # RETURN_MESSAGE marker
# RETURN_MESSAGE,3 @ 70169B82
push 70169B81 740075C6 3   # WPSH args1-3
call 70169B82 740075CC   # RETURN_MESSAGE marker
# RETURN_MESSAGE,6 @ 70175EC8
push 70175EC5 740075CA   # LPEF arg1
push 70175EC2 740075C8   # LPEF arg2
push 70175EC0 740075C6   # XPEF arg3
push 70175EBE 740075C2 2   # WPSH args4-5
push 70175EBC 740075C0   # XPEF arg6
call 70175EC8 740075CC   # RETURN_MESSAGE marker
# RETURN_MESSAGE,6 @ 70175EFF
push 70175EFD 740075CA   # XPEF arg1
push 70175EFA 740075C8   # LPEF arg2
push 70175EF8 740075C6   # XPEF arg3
push 70175EF6 740075C2 2   # WPSH args4-5
push 70175EF4 740075C0   # XPEF arg6
call 70175EFF 740075CC   # RETURN_MESSAGE marker
# RETURN_MESSAGE,6 @ 7017D7D9
push 7017D7D7 740075CA   # XPEF arg1
push 7017D7D4 740075C8   # LPEF arg2
push 7017D7D2 740075C6   # XPEF arg3
push 7017D7D0 740075C2 2   # WPSH args4-5
push 7017D7CE 740075C0   # XPEF arg6
call 7017D7D9 740075CC   # RETURN_MESSAGE marker
