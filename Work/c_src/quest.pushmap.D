# M4b caller map — Project 19 tranche D: 5 RETURN_MESSAGE sites (all
# LCALL; no new code). 70169B82 is the pass-by-reference site: its
# WPSH 0,2 pushes three POINTER values to stack temporaries — the
# pointers are redirected, the temps stay on the stack. RETURN_MESSAGE
# never WRTNs (ends in SYSCALL 0310, retired) — expect a write-mode
# WSAVS/WRTN imbalance of +1 per firing leg.
# RETURN_MESSAGE,6 @ 7015BE74
push 7015BE72 74007626   # XPEF arg1
push 7015BE6F 74007624   # LPEF arg2
push 7015BE6D 74007622   # XPEF arg3
push 7015BE6B 7400761E 2   # WPSH args4-5
push 7015BE69 7400761C   # XPEF arg6
call 7015BE74 74007628   # RETURN_MESSAGE marker
# RETURN_MESSAGE,3 @ 70169B82
push 70169B81 74007622 3   # WPSH args1-3
call 70169B82 74007628   # RETURN_MESSAGE marker
# RETURN_MESSAGE,6 @ 70175EC8
push 70175EC5 74007626   # LPEF arg1
push 70175EC2 74007624   # LPEF arg2
push 70175EC0 74007622   # XPEF arg3
push 70175EBE 7400761E 2   # WPSH args4-5
push 70175EBC 7400761C   # XPEF arg6
call 70175EC8 74007628   # RETURN_MESSAGE marker
# RETURN_MESSAGE,6 @ 70175EFF
push 70175EFD 74007626   # XPEF arg1
push 70175EFA 74007624   # LPEF arg2
push 70175EF8 74007622   # XPEF arg3
push 70175EF6 7400761E 2   # WPSH args4-5
push 70175EF4 7400761C   # XPEF arg6
call 70175EFF 74007628   # RETURN_MESSAGE marker
# RETURN_MESSAGE,6 @ 7017D7D9
push 7017D7D7 74007626   # XPEF arg1
push 7017D7D4 74007624   # LPEF arg2
push 7017D7D2 74007622   # XPEF arg3
push 7017D7D0 7400761E 2   # WPSH args4-5
push 7017D7CE 7400761C   # XPEF arg6
call 7017D7D9 74007628   # RETURN_MESSAGE marker
