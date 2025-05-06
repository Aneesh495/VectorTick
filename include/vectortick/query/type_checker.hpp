#pragma once

#include "ast.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include "../storage/file_format.hpp"

namespace vectortick {
namespace query {

class TypeChecker {
public:
    TypeChecker() = default;
    
    // Validate a query statement against the canonical schema
    [[nodiscard]] Status check_query(const QueryStmt* stmt) const noexcept;
    
    // Check expression and return its inferred type
    [[nodiscard]] Result<vts1::ColumnType> check_expression(const Expression* expr) const noexcept;
    
    // Resolve column name to canonical ColumnID
    [[nodiscard]] static bool resolve_column(std::string_view name, vts1::ColumnID& out_id) noexcept;
};

} // namespace query
} // namespace vectortick
