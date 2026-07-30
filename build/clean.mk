# ── Clean Target ──────────────────────────────────────────────────
# Included by top-level Makefile

.PHONY: clean clean-tests

clean:
	rm -f slermes src/*.o src/*/*.o src/*/*/*.o
	rm -f $(LIB_OBJ) $(LIB_A)
	rm -rf digest_output/
	rm -f *.gcda *.gcno src/*.gcda src/*.gcno src/*/*.gcda src/*/*.gcno
	rm -f lib/*.gcda lib/*.gcno lib/*/*.gcda lib/*/*.gcno
	rm -rf coverage_html/ coverage.info coverage-filtered.info
