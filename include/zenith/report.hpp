#pragma once

#include <string>

namespace zenith {

class IssueReportGenerator {
public:
    // Generates an anonymized markdown report ready for a GitHub Issue
    static std::string generate_anonymized_report();

    // Sanitizes text by stripping usernames, hostnames, and IP addresses
    static std::string sanitize_text(const std::string& input);
};

} // namespace zenith
