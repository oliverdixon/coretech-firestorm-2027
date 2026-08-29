# Integration Tests

The Python integration tests under `stage1/` and `stage2/` are provided by CoreTech.

`run_integration.py` is a minimal command-line helper to execute the streamer, provided by CoreTech, and the _FIRE
STORM_ application in tandem. Following a successful invocation, the PDF documents produced by _FIRE STORM_ are verified
by SHA256 to match the reference files.

## Changes

In both `stage1/test.py` and `stage2/test.py`, a single line was changed:

```python
unittest.main(exit=False)
```

was replaced with

```python
unittest.main()
```

such that `unittest` failures propagated through the return code to the shell. This allows accurate reporting in CTest
and CI.
