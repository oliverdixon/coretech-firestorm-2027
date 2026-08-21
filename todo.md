* Fail on truncated input.
* Verify PDF integrity (test for '%PDF' etc, and verify that there are no gaps in the captured sequence).
* Address padding validation.
* Use vector instead of set/map.
* Bulk insertion instead of back_inserter.
* Add connection timeouts.
* Add unit tests.
* Integrate unit tests into CI.
* Re-add decoder finalisation.
