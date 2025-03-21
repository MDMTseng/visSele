# cJSONPP Tests

This directory contains tests for the cJSONPP library, which is a C++ wrapper for cJSON.

## Test Files

- `test_cJsonPP.cpp`: Basic tests for common operations
- `test_advanced.cpp`: Advanced tests for complex operations and edge cases

## Running Tests

From the main cJSONPP directory, run:

```bash
make test
```

This will:
1. Build the cJSONPP library
2. Build all test executables
3. Run all tests and report results

## Adding New Tests

To add a new test:

1. Create a new `.cpp` file in this directory
2. Include the `cJsonPP.h` header
3. Implement your test with a standard `main()` function
4. Return 0 for success, non-zero for failure

The build system will automatically detect and build your test file.

## Testing Standards

When writing tests, please follow these standards:

- Each test file should focus on testing a specific aspect of cJSONPP
- Use the `ASSERT_TEST` macro to validate test conditions
- Test both success cases and failure/edge cases
- Clean up any resources created during the test
- Clearly document the purpose of each test file

## Notes for Test Development

- The test Makefile handles compilation and linking against the cJSONPP library
- All tests must be able to run independently
- Keep individual tests focused and efficient 