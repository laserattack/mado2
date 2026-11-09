#!/usr/bin/env python3

import argparse
from datetime import datetime, timedelta, timezone
from pathlib import Path
import sys

DEFAULT_COUNT = 10000
BASE_DIR = "MADO"
ENTRY_FILE = "MAIN.md"
START_DATE = datetime(2026, 1, 1, tzinfo=timezone.utc)
ENTRY_INTERVAL = timedelta(minutes=1)
TIMESTAMP_FORMAT = "%Y%m%d-%H%M%S"
PROGRESS_PARTS = 100
MAX_COUNT_DIGITS = 9
ENTRY_HEADER = (
    "- NAME: request confirmation during repository initialization\n"
    "- PRIORITY: 100\n"
    "- TAGS: feat, ux\n"
    "- STATUS: closed\n"
)
LONG_TEXT = """## Description
This is a detailed description of the entry. It contains multiple paragraphs
to simulate real-world usage of the mado system with substantial content.

## Implementation Details
The implementation follows standard practices with proper error handling
and edge case consideration. All tests pass successfully.

## Notes
- First observation about the implementation
- Second point regarding performance optimization
- Additional consideration for future improvements
- Security review completed and approved
- Documentation has been updated accordingly

## Technical Specifications
The system uses a modular architecture with clear separation of concerns.
Each component is independently testable and follows the single responsibility
principle. The codebase maintains high test coverage and passes all CI checks.

## Review Comments
The code review was conducted on multiple aspects including functionality,
performance, security, and maintainability. All identified issues have been
addressed and verified. The solution demonstrates good engineering practices.

## Performance Metrics
- Response time: < 100ms
- Memory usage: optimized
- CPU utilization: within acceptable limits
- Scalability: tested up to 10x current load

## Dependencies
- Core library v2.1.0 or higher
- Utility package latest stable
- Development tools as specified in documentation

## Changelog
- Added new feature for improved user experience
- Fixed edge case in data processing pipeline
- Updated documentation with examples
- Refactored core module for better maintainability
- Enhanced error messages for debugging

## Configuration
The configuration is straightforward and well-documented. Default values
are sensible and work for most use cases. Advanced options are available
for specific requirements.

## Testing
Comprehensive test suite covers unit tests, integration tests, and
end-to-end scenarios. Regression tests ensure backward compatibility.
Performance benchmarks validate optimization efforts.

## Deployment
Deployment process is automated and includes validation steps. Rollback
procedures are tested and documented. Monitoring alerts are configured
for early detection of issues.

## Support
Documentation is available in the wiki. Team members are available for
questions during business hours. Emergency support is available 24/7
for critical issues."""


def parse_count(value):
    if not value.isascii() or not value.isdecimal() or len(value) > MAX_COUNT_DIGITS:
        raise argparse.ArgumentTypeError(
            f"expected a nonnegative integer with at most {MAX_COUNT_DIGITS} digits")
    return int(value)


def generate_entries(count):
    step = max(count // PROGRESS_PARTS, 1)
    base_dir = Path(BASE_DIR)

    if count:
        base_dir.mkdir(exist_ok=True)

    for index in range(1, count + 1):
        timestamp = START_DATE + index * ENTRY_INTERVAL
        entry_dir = base_dir / timestamp.strftime(TIMESTAMP_FORMAT)
        entry_dir.mkdir()
        content = (
            f"{ENTRY_HEADER}\n{LONG_TEXT}\n\n"
            f"Entry ID: test-{index}\n"
            f"Generated: {datetime.now(timezone.utc).isoformat()}\n"
        )

        with (entry_dir / ENTRY_FILE).open("x", encoding="utf-8", newline="\n") as output:
            output.write(content)

        if index % step == 0:
            print(f"Progress: {index * PROGRESS_PARTS // count}% ({index}/{count} entries created)...")

    print(f"Done! Created {count} test entries.")


def main():
    parser = argparse.ArgumentParser(description="Generate sample MADO entries in the current directory")
    parser.add_argument("count", nargs="?", default=DEFAULT_COUNT, type=parse_count)
    args = parser.parse_args()

    try:
        generate_entries(args.count)
    except OSError as error:
        print(error, file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
