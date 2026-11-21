#include "test_utils.hpp"

#include <mado/entry/entry.hpp>

#include <limits>

using namespace mado::entry;

static void test_entry_defaults() {
    const Entry entry;

    test(entry.path().empty() && entry.name().empty() && entry.body().empty() &&
         entry.status().empty() && entry.priority() == 0 &&
         entry.tags() == std::vector<std::string>{""} &&
         entry.time() == mado::common::NEVER_TIMESTAMP &&
         entry.mtime() == mado::common::NEVER_TIMESTAMP &&
         entry.deadline() == mado::common::NEVER_TIMESTAMP);
}

static void test_priority_boundaries() {
    Entry entry;
    entry.set_priority(MAX_PRIORITY);
    bool correct = entry.priority() == MAX_PRIORITY;

    for (const auto value : {uint16_t(MAX_PRIORITY + 1), std::numeric_limits<uint16_t>::max()}) {
        bool rejected = false;
        try {
            entry.set_priority(value);
        } catch (const Entry_Error &) {
            rejected = true;
        }
        correct &= rejected && entry.priority() == MAX_PRIORITY;
    }

    entry.set_priority(0);
    test(correct && entry.priority() == 0);
}

static void test_empty_tags_preserve_value() {
    Entry entry;
    entry.set_tags({"one", "two"});
    bool rejected = false;
    try {
        entry.set_tags({});
    } catch (const Entry_Error &) {
        rejected = true;
    }
    const bool preserved = entry.tags() == std::vector<std::string>{"one", "two"};

    entry.set_tags({""});
    test(rejected && preserved && entry.tags() == std::vector<std::string>{""});
}

static void test_timestamp_setters() {
    using Setter = void (Entry::*)(std::string);
    using Getter = const std::string &(Entry::*)() const;
    struct Timestamp_Field {
        Setter set;
        Getter get;
    };
    const Timestamp_Field fields[] = {
        {&Entry::set_time, &Entry::time},
        {&Entry::set_mtime, &Entry::mtime},
        {&Entry::set_deadline, &Entry::deadline},
    };

    bool correct = true;
    for (const auto &field : fields) {
        Entry entry;
        for (const std::string value : {"2024", "202402", "20240229", "20240229-",
                                        "20240229-23", "20240229-2359", "20240229-235959"}) {
            (entry.*field.set)(value);
            correct &= (entry.*field.get)() == value;
        }
        const auto previous = (entry.*field.get)();
        for (const std::string value : {"", "not-a-date", "20230229", "20240229-240000"}) {
            bool rejected = false;
            try {
                (entry.*field.set)(value);
            } catch (const Entry_Error &) {
                rejected = true;
            }
            correct &= rejected && (entry.*field.get)() == previous;
        }
    }
    test(correct);
}

int main() {
    const std::vector<Test_Case> tests = {
        {"Defaults", "empty metadata and unset timestamps", test_entry_defaults},
        {"Priority boundaries", "accept limits and preserve priority on rejection", test_priority_boundaries},
        {"Empty tags", "reject empty vectors and accept the no-tags sentinel", test_empty_tags_preserve_value},
        {"Timestamp setters", "accept supported forms and preserve values on rejection", test_timestamp_setters},
    };
    return run_tests(tests, "entry: ");
}
