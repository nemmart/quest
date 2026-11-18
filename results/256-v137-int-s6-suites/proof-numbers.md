Traceback (most recent call last):
  File "/tmp/v137-256-v137-int-s6-suites/kit/v1.37_wip/tests/proof_numbers.py", line 104, in <module>
    main()
  File "/tmp/v137-256-v137-int-s6-suites/kit/v1.37_wip/tests/proof_numbers.py", line 73, in main
    rows = [r for p in sys.argv[1:] if p != "--frames" for r in runs(p)]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/tmp/v137-256-v137-int-s6-suites/kit/v1.37_wip/tests/proof_numbers.py", line 24, in runs
    for line in open(path, errors="replace"):
                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
FileNotFoundError: [Errno 2] No such file or directory: 'results/proof-real.log'
