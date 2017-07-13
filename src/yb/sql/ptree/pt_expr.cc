//--------------------------------------------------------------------------------------------------
// Copyright (c) YugaByte, Inc.
//
// Treenode definitions for expressions.
//--------------------------------------------------------------------------------------------------

#include "yb/sql/ptree/pt_expr.h"
#include "yb/sql/ptree/pt_bcall.h"
#include "yb/sql/ptree/sem_context.h"
#include "yb/util/decimal.h"
#include "yb/util/net/inetaddress.h"

namespace yb {
namespace sql {

using client::YBColumnSchema;
using std::shared_ptr;

//--------------------------------------------------------------------------------------------------

CHECKED_STATUS PTExpr::CheckOperator(SemContext *sem_context) {
  // Where clause only allow AND, EQ, LT, LE, GT, and GE operators.
  if (sem_context->where_state() != nullptr) {
    switch (yql_op_) {
      case YQL_OP_AND:
      case YQL_OP_EQUAL:
      case YQL_OP_LESS_THAN:
      case YQL_OP_LESS_THAN_EQUAL:
      case YQL_OP_GREATER_THAN:
      case YQL_OP_GREATER_THAN_EQUAL:
      case YQL_OP_NOOP:
        break;
      default:
        return sem_context->Error(loc(), "This operator is not allowed in where clause",
                                  ErrorCode::CQL_STATEMENT_INVALID);
    }
  }
  return Status::OK();
}

CHECKED_STATUS PTExpr::AnalyzeOperator(SemContext *sem_context) {
  return Status::OK();
}

CHECKED_STATUS PTExpr::AnalyzeOperator(SemContext *sem_context,
                                       PTExpr::SharedPtr op1) {
  return Status::OK();
}

CHECKED_STATUS PTExpr::AnalyzeOperator(SemContext *sem_context,
                                       PTExpr::SharedPtr op1,
                                       PTExpr::SharedPtr op2) {
  return Status::OK();
}

CHECKED_STATUS PTExpr::AnalyzeOperator(SemContext *sem_context,
                                       PTExpr::SharedPtr op1,
                                       PTExpr::SharedPtr op2,
                                       PTExpr::SharedPtr op3) {
  return Status::OK();
}

CHECKED_STATUS PTExpr::SetupSemStateForOp1(SemState *sem_state) {
  return Status::OK();
}

CHECKED_STATUS PTExpr::SetupSemStateForOp2(SemState *sem_state) {
  // Passing down where clause state variables.
  return Status::OK();
}

CHECKED_STATUS PTExpr::SetupSemStateForOp3(SemState *sem_state) {
  return Status::OK();
}

CHECKED_STATUS PTExpr::CheckExpectedTypeCompatibility(SemContext *sem_context) {
  CHECK(has_valid_internal_type() && has_valid_yql_type_id());

  // Check if RHS support counter update.
  if (sem_context->updating_counter() != nullptr) {
    RETURN_NOT_OK(this->CheckCounterUpdateSupport(sem_context));
  }

  // Check if RHS is convertible to LHS.
  if (!sem_context->expr_expected_yql_type()->IsUnknown()) {
    if (!sem_context->IsConvertible(this, sem_context->expr_expected_yql_type())) {
      return sem_context->Error(loc(), ErrorCode::DATATYPE_MISMATCH);
    }
  }

  // Resolve internal type.
  const InternalType expected_itype = sem_context->expr_expected_internal_type();
  if (expected_itype == InternalType::VALUE_NOT_SET) {
    expected_internal_type_ = internal_type_;
  } else {
    expected_internal_type_ = expected_itype;
  }
  return Status::OK();
}

//--------------------------------------------------------------------------------------------------

CHECKED_STATUS PTExpr::AnalyzeLeftRightOperands(SemContext *sem_context,
                                                PTExpr::SharedPtr lhs,
                                                PTExpr::SharedPtr rhs) {
  if (!sem_context->IsComparable(lhs->yql_type_id(), rhs->yql_type_id())) {
    return sem_context->Error(loc(), "Cannot compare values of these datatypes",
                              ErrorCode::INCOMPARABLE_DATATYPES);
  }
  return Status::OK();
}

CHECKED_STATUS PTExpr::CheckLhsExpr(SemContext *sem_context) {
  if (op_ != ExprOperator::kRef && op_ != ExprOperator::kBcall) {
    return sem_context->Error(loc(),
                              "Only column refs and builtin calls are allowed for left hand value",
                              ErrorCode::CQL_STATEMENT_INVALID);
  }
  return Status::OK();
}

CHECKED_STATUS PTExpr::CheckRhsExpr(SemContext *sem_context) {
  // Check for limitation in YQL (Not all expressions are acceptable).
  switch (op_) {
    case ExprOperator::kConst: FALLTHROUGH_INTENDED;
    case ExprOperator::kCollection: FALLTHROUGH_INTENDED;
    case ExprOperator::kUMinus: FALLTHROUGH_INTENDED;
    case ExprOperator::kBindVar: FALLTHROUGH_INTENDED;
    case ExprOperator::kRef: FALLTHROUGH_INTENDED;
    case ExprOperator::kBcall:
      break;
    default:
      return sem_context->Error(loc(), "Operator not allowed as right hand value",
                                ErrorCode::CQL_STATEMENT_INVALID);
  }
  return Status::OK();
}

CHECKED_STATUS PTExpr::CheckCounterUpdateSupport(SemContext *sem_context) const {
  return sem_context->Error(loc(), ErrorCode::INVALID_COUNTING_EXPR);
}

//--------------------------------------------------------------------------------------------------

PTLiteralString::PTLiteralString(MCSharedPtr<MCString> value)
    : PTLiteral<MCSharedPtr<MCString>>(value) {
}

PTLiteralString::~PTLiteralString() {
}

CHECKED_STATUS PTLiteralString::ToInt64(int64_t *value, bool negate) const {
  if (negate) {
    string negate = string("-") + value_->c_str();
    *value = std::stol(negate.c_str());
  } else {
    *value = std::stol(value_->c_str());
  }
  return Status::OK();
}

CHECKED_STATUS PTLiteralString::ToDouble(long double *value, bool negate) const {
  if (negate) {
    *value = -std::stold(value_->c_str());
  } else {
    *value = std::stold(value_->c_str());
  }
  return Status::OK();
}

CHECKED_STATUS PTLiteralString::ToDecimal(util::Decimal *value, bool negate) const {
  if (negate) {
    return value->FromString(string("-") + value_->c_str());
  } else {
    return value->FromString(value_->c_str());
  }
}

CHECKED_STATUS PTLiteralString::ToDecimal(string *value, bool negate) const {
  util::Decimal d;
  if (negate) {
    RETURN_NOT_OK(d.FromString(string("-") + value_->c_str()));
  } else {
    RETURN_NOT_OK(d.FromString(value_->c_str()));
  }
  *value = d.EncodeToComparable();
  return Status::OK();
}

CHECKED_STATUS PTLiteralString::ToString(string *value) const {
  *value = value_->c_str();
  return Status::OK();
}

CHECKED_STATUS PTLiteralString::ToTimestamp(int64_t *value) const {
  Timestamp ts;
  RETURN_NOT_OK(DateTime::TimestampFromString(value_->c_str(), &ts));
  *value = ts.ToInt64();
  return Status::OK();
}

CHECKED_STATUS PTLiteralString::ToInetaddress(InetAddress *value) const {
  RETURN_NOT_OK(value->FromString(value_->c_str()));
  return Status::OK();
}

//--------------------------------------------------------------------------------------------------
// Collection constants.
CHECKED_STATUS PTMapExpr::Analyze(SemContext *sem_context) {
  RETURN_NOT_OK(CheckOperator(sem_context));

  // Run semantic analysis on collection elements.
  const shared_ptr<YQLType>& expected_type = sem_context->expr_expected_yql_type();
  if (expected_type->main() != DataType::MAP) {
    return sem_context->Error(loc(), ErrorCode::DATATYPE_MISMATCH);
  }

  // Analyze key and value.
  SemState sem_state(sem_context);

  // TODO(mihnea) Need to take care of expected InternalType here also.
  const shared_ptr<YQLType>& key_type = expected_type->param_type(0);
  sem_state.SetExprState(key_type, YBColumnSchema::ToInternalDataType(key_type));
  for (auto& key : keys_) {
    RETURN_NOT_OK(key->Analyze(sem_context));
  }

  // TODO(mihnea) Need to take care of expected InternalType here also.
  const shared_ptr<YQLType>& value_type = expected_type->param_type(1);
  sem_state.SetExprState(value_type, YBColumnSchema::ToInternalDataType(value_type));
  for (auto& value : values_) {
    RETURN_NOT_OK(value->Analyze(sem_context));
  }

  // Assign correct datatype.
  yql_type_ = expected_type;

  // TODO(mihnea) Destructor ~SemState() will reset the state, but if you need to reset the state
  // before returning, call ResetContextState() (see the usage in pt_expr.h).
  sem_state.ResetContextState();
  return CheckExpectedTypeCompatibility(sem_context);
}

CHECKED_STATUS PTSetExpr::Analyze(SemContext *sem_context) {
  RETURN_NOT_OK(CheckOperator(sem_context));

  // Run semantic analysis on collection elements.
  const shared_ptr<YQLType> &expected_type = sem_context->expr_expected_yql_type();
  if (expected_type->main() != DataType::SET &&
      (expected_type->main() != DataType::MAP || value_.size() != 0)) {
    return sem_context->Error(loc(), ErrorCode::DATATYPE_MISMATCH);
  }

  SemState sem_state(sem_context);
  const shared_ptr<YQLType>& value_type = expected_type->param_type(0);
  sem_state.SetExprState(value_type, YBColumnSchema::ToInternalDataType(value_type));
  for (auto& elem : value_) {
    RETURN_NOT_OK(elem->Analyze(sem_context));
  }

  // TODO(Mihnea) Because our current code (including executor) typecast this expression based on
  // its datatype, I cannot change its expected type from SET to MAP. The following if block
  // only allow type-switching between SETs but not between SET and MAP.
  if (expected_type->main() == DataType::SET) {
    yql_type_ = expected_type;
  }

  // TODO(mihnea) Destructor ~SemState() will reset the state, but if you need to reset the state
  // before returning, call ResetContextState() (see the usage in pt_expr.h).
  sem_state.ResetContextState();
  return CheckExpectedTypeCompatibility(sem_context);
}

CHECKED_STATUS PTListExpr::Analyze(SemContext *sem_context) {
  RETURN_NOT_OK(CheckOperator(sem_context));

  // Run semantic analysis on collection elements.
  const shared_ptr<YQLType>& expected_type = sem_context->expr_expected_yql_type();
  if (expected_type->main() != DataType::LIST) {
    return sem_context->Error(loc(), ErrorCode::DATATYPE_MISMATCH);
  }

  SemState sem_state(sem_context);
  const shared_ptr<YQLType>& value_type = expected_type->param_type(0);
  sem_state.SetExprState(value_type, YBColumnSchema::ToInternalDataType(value_type));
  for (auto& elem : value_) {
    RETURN_NOT_OK(elem->Analyze(sem_context));
  }
  yql_type_ = expected_type;

  // TODO(mihnea) Destructor ~SemState() will reset the state, but if you need to reset the state
  // before returning, call ResetContextState() (see the usage in pt_expr.h).
  sem_state.ResetContextState();
  return CheckExpectedTypeCompatibility(sem_context);
}

//--------------------------------------------------------------------------------------------------
// Logic expressions consist of the following operators.
//   ExprOperator::kNot
//   ExprOperator::kAND
//   ExprOperator::kOR
//   ExprOperator::kIsTrue
//   ExprOperator::kIsFalse

CHECKED_STATUS PTLogicExpr::SetupSemStateForOp1(SemState *sem_state) {
  // Expect "bool" datatype for logic expression.
  sem_state->SetExprState(DataType::BOOL, InternalType::kBoolValue);

  // If this is OP_AND, we need to pass down the state variables for where clause "where_state".
  if (yql_op_ == YQL_OP_AND) {
    sem_state->CopyPreviousWhereState();
  }
  return Status::OK();
}

CHECKED_STATUS PTLogicExpr::SetupSemStateForOp2(SemState *sem_state) {
  // Expect "bool" datatype for logic expression.
  sem_state->SetExprState(DataType::BOOL, InternalType::kBoolValue);

  // If this is OP_AND, we need to pass down the state variables for where clause "where_state".
  if (yql_op_ == YQL_OP_AND) {
    sem_state->CopyPreviousWhereState();
  }
  return Status::OK();
}

CHECKED_STATUS PTLogicExpr::AnalyzeOperator(SemContext *sem_context,
                                            PTExpr::SharedPtr op1) {
  switch (yql_op_) {
    case YQL_OP_NOT:
      if (op1->yql_type_id() != BOOL) {
        return sem_context->Error(loc(), "Only boolean value is allowed in this context",
                                  ErrorCode::INVALID_DATATYPE);
      }
      internal_type_ = yb::InternalType::kBoolValue;
      break;

    case YQL_OP_IS_TRUE: FALLTHROUGH_INTENDED;
    case YQL_OP_IS_FALSE:
      return sem_context->Error(loc(), "Operator not supported yet",
                                ErrorCode::CQL_STATEMENT_INVALID);
    default:
      LOG(FATAL) << "Invalid operator";
  }
  return Status::OK();
}

CHECKED_STATUS PTLogicExpr::AnalyzeOperator(SemContext *sem_context,
                                            PTExpr::SharedPtr op1,
                                            PTExpr::SharedPtr op2) {
  // Verify the operators.
  DCHECK(yql_op_ == YQL_OP_AND || yql_op_ == YQL_OP_OR);

  // "op1" and "op2" must have been analyzed before getting here
  if (op1->yql_type_id() != BOOL) {
    return sem_context->Error(op1->loc(), "Only boolean value is allowed in this context",
                              ErrorCode::INVALID_DATATYPE);
  }
  if (op2->yql_type_id() != BOOL) {
    return sem_context->Error(op2->loc(), "Only boolean value is allowed in this context",
                              ErrorCode::INVALID_DATATYPE);
  }

  internal_type_ = yb::InternalType::kBoolValue;
  return Status::OK();
}

//--------------------------------------------------------------------------------------------------
// Relations expressions: ==, !=, >, >=, between, ...

CHECKED_STATUS PTRelationExpr::SetupSemStateForOp1(SemState *sem_state) {
  // passing down where state
  sem_state->CopyPreviousWhereState();
  // No expectation for operand 1. All types are accepted.
  return Status::OK();
}

CHECKED_STATUS PTRelationExpr::SetupSemStateForOp2(SemState *sem_state) {
  // The states of operand2 is dependent on operand1.
  PTExpr::SharedPtr operand1 = op1();
  DCHECK(operand1 != nullptr);

  if (operand1->expr_op() == ExprOperator::kRef) {
    const PTRef *ref = static_cast<const PTRef*>(operand1.get());
    sem_state->SetExprState(ref->yql_type(),
                            ref->internal_type(),
                            ref->bindvar_name(),
                            ref->desc()->is_hash() ? ref->desc() : nullptr);
  } else {
    sem_state->SetExprState(operand1->yql_type(), operand1->internal_type());
  }

  if (operand1->expr_op() == ExprOperator::kBcall) {
    PTBcall* bcall = static_cast<PTBcall *>(operand1.get());
    if (strcmp(bcall->name()->c_str(), "token") == 0) {
      sem_state->set_bindvar_name(PTBindVar::token_bindvar_name());
    }
  }

  return Status::OK();
}

CHECKED_STATUS PTRelationExpr::SetupSemStateForOp3(SemState *sem_state) {
  // The states of operand3 is dependend on operand1 in the same way as op2.
  return SetupSemStateForOp2(sem_state);
}

CHECKED_STATUS PTRelationExpr::AnalyzeOperator(SemContext *sem_context) {
  switch (yql_op_) {
    case YQL_OP_EXISTS: FALLTHROUGH_INTENDED;
    case YQL_OP_NOT_EXISTS:
      return Status::OK();
    default:
      LOG(FATAL) << "Invalid operator";
  }
  return Status::OK();
}

CHECKED_STATUS PTRelationExpr::AnalyzeOperator(SemContext *sem_context,
                                               PTExpr::SharedPtr op1) {
  // "op1" must have been analyzed before getting here
  switch (yql_op_) {
    case YQL_OP_IS_NULL: FALLTHROUGH_INTENDED;
    case YQL_OP_IS_NOT_NULL:
      return sem_context->Error(loc(), "Operator not supported yet",
                                ErrorCode::CQL_STATEMENT_INVALID);
    default:
      LOG(FATAL) << "Invalid operator" << int(yql_op_);
  }

  return Status::OK();
}

CHECKED_STATUS PTRelationExpr::AnalyzeOperator(SemContext *sem_context,
                                               PTExpr::SharedPtr op1,
                                               PTExpr::SharedPtr op2) {
  // "op1" and "op2" must have been analyzed before getting here
  switch (yql_op_) {
    case YQL_OP_EQUAL: FALLTHROUGH_INTENDED;
    case YQL_OP_LESS_THAN: FALLTHROUGH_INTENDED;
    case YQL_OP_GREATER_THAN: FALLTHROUGH_INTENDED;
    case YQL_OP_LESS_THAN_EQUAL: FALLTHROUGH_INTENDED;
    case YQL_OP_GREATER_THAN_EQUAL: FALLTHROUGH_INTENDED;
    case YQL_OP_NOT_EQUAL:
      RETURN_NOT_OK(op1->CheckLhsExpr(sem_context));
      RETURN_NOT_OK(op2->CheckRhsExpr(sem_context));
      RETURN_NOT_OK(AnalyzeLeftRightOperands(sem_context, op1, op2));
      internal_type_ = yb::InternalType::kBoolValue;
      break;

    default:
      return sem_context->Error(loc(), "Operator not supported yet",
          ErrorCode::CQL_STATEMENT_INVALID);
  }

  WhereExprState *where_state = sem_context->where_state();
  if (where_state != nullptr) {
    // CheckLhsExpr already checks that this is either kRef or kBcall
    DCHECK(op1->expr_op() == ExprOperator::kRef || op1->expr_op() == ExprOperator::kBcall);
    if (op1->expr_op() == ExprOperator::kRef) {
      const PTRef *ref = static_cast<const PTRef *>(op1.get());
      return where_state->AnalyzeColumnOp(sem_context, this, ref->desc(), op2);
    } else if (op1->expr_op() == ExprOperator::kBcall) {
      const PTBcall *bcall = static_cast<const PTBcall *>(op1.get());
      if (strcmp(bcall->name()->c_str(), "token") == 0) {
        const PTToken *token = static_cast<const PTToken *>(bcall);
        if (token->is_partition_key_ref()) {
          return where_state->AnalyzePartitionKeyOp(sem_context, this, op2);
        } else {
          return sem_context->Error(loc(), "Only token calls that reference partition key allowed",
              ErrorCode::FEATURE_NOT_SUPPORTED);
        }
      } else {
        return sem_context->Error(loc(), "Expected token found different builtin call",
            ErrorCode::CQL_STATEMENT_INVALID);
      }
    }
  }
  return Status::OK();
}

CHECKED_STATUS PTRelationExpr::AnalyzeOperator(SemContext *sem_context,
                                               PTExpr::SharedPtr op1,
                                               PTExpr::SharedPtr op2,
                                               PTExpr::SharedPtr op3) {
  // "op1", "op2", and "op3" must have been analyzed before getting here
  switch (yql_op_) {
    case YQL_OP_BETWEEN: FALLTHROUGH_INTENDED;
    case YQL_OP_NOT_BETWEEN:
      RETURN_NOT_OK(op1->CheckLhsExpr(sem_context));
      RETURN_NOT_OK(op2->CheckRhsExpr(sem_context));
      RETURN_NOT_OK(op3->CheckRhsExpr(sem_context));
      RETURN_NOT_OK(AnalyzeLeftRightOperands(sem_context, op1, op2));
      RETURN_NOT_OK(AnalyzeLeftRightOperands(sem_context, op1, op3));
      internal_type_ = yb::InternalType::kBoolValue;
      break;

    default:
      LOG(FATAL) << "Invalid operator";
  }

  return Status::OK();
}

//--------------------------------------------------------------------------------------------------

CHECKED_STATUS PTOperatorExpr::SetupSemStateForOp1(SemState *sem_state) {
  switch (op_) {
    case ExprOperator::kUMinus:
      sem_state->CopyPreviousStates();
      break;
    default:
      LOG(FATAL) << "Invalid operator" << int(op_);
  }

  return Status::OK();
}

CHECKED_STATUS PTOperatorExpr::AnalyzeOperator(SemContext *sem_context,
                                               PTExpr::SharedPtr op1) {
  switch (op_) {
    case ExprOperator::kUMinus:
      // "op1" must have been analyzed before we get here.
      // Check to make sure that it is allowed in this context.
      if (op1->expr_op() != ExprOperator::kConst) {
        return sem_context->Error(loc(), "Only numeric constant is allowed in this context",
                                  ErrorCode::FEATURE_NOT_SUPPORTED);
      }
      if (!YQLType::IsNumeric(op1->yql_type_id())) {
        return sem_context->Error(loc(), "Only numeric data type is allowed in this context",
                                  ErrorCode::INVALID_DATATYPE);
      }

      // Type resolution: (-x) should have the same datatype as (x).
      yql_type_ = op1->yql_type();
      internal_type_ = op1->internal_type();
      break;

    default:
      LOG(FATAL) << "Invalid operator" << int(op_);
  }

  return Status::OK();
}

//--------------------------------------------------------------------------------------------------

PTRef::PTRef(MemoryContext *memctx,
             YBLocation::SharedPtr loc,
             const PTQualifiedName::SharedPtr& name)
    : PTOperator0(memctx, loc, ExprOperator::kRef, yb::YQLOperator::YQL_OP_NOOP),
      name_(name),
      desc_(nullptr) {
}

PTRef::~PTRef() {
}

CHECKED_STATUS PTRef::AnalyzeOperator(SemContext *sem_context) {

  // Check if this refers to the whole table (SELECT *).
  if (name_ == nullptr) {
    return sem_context->Error(loc(), "Cannot do type resolution for wildcard reference (SELECT *)");
  }

  // Look for a column descriptor from symbol table.
  RETURN_NOT_OK(name_->Analyze(sem_context));
  desc_ = sem_context->GetColumnDesc(name_->last_name(), true /* reading_column */);
  if (desc_ == nullptr) {
    return sem_context->Error(loc(), "Column doesn't exist", ErrorCode::UNDEFINED_COLUMN);
  }

  // Type resolution: Ref(x) should have the same datatype as (x).
  internal_type_ = desc_->internal_type();
  yql_type_ = desc_->yql_type();

  return Status::OK();
}

CHECKED_STATUS PTRef::CheckLhsExpr(SemContext *sem_context) {
  // When CQL IF clause is being processed. In that case, disallow reference to primary key columns
  // and counters.
  if (sem_context->processing_if_clause()) {
    if (desc_->is_primary()) {
      return sem_context->Error(loc(), "Primary key column reference is not allowed in if clause",
                                ErrorCode::CQL_STATEMENT_INVALID);
    } else if (desc_->is_counter()) {
      return sem_context->Error(loc(), "Counter column reference is not allowed in if clause",
                                ErrorCode::CQL_STATEMENT_INVALID);
    }
  }
  return Status::OK();
}

void PTRef::PrintSemanticAnalysisResult(SemContext *sem_context) {
  VLOG(3) << "SEMANTIC ANALYSIS RESULT (" << *loc_ << "):\n" << "Not yet avail";
}

//--------------------------------------------------------------------------------------------------

PTExprAlias::PTExprAlias(MemoryContext *memctx,
                         YBLocation::SharedPtr loc,
                         const PTExpr::SharedPtr& expr,
                         const MCSharedPtr<MCString>& alias)
    : PTOperator1(memctx, loc, ExprOperator::kAlias, yb::YQLOperator::YQL_OP_NOOP, expr),
      alias_(alias) {
}

PTExprAlias::~PTExprAlias() {
}

CHECKED_STATUS PTExprAlias::AnalyzeOperator(SemContext *sem_context, PTExpr::SharedPtr op1) {
  // Type resolution: Alias of (x) should have the same datatype as (x).
  yql_type_ = op1->yql_type();
  internal_type_ = op1->internal_type();

  return Status::OK();
}

//--------------------------------------------------------------------------------------------------

PTBindVar::PTBindVar(MemoryContext *memctx,
                     YBLocation::SharedPtr loc,
                     const MCSharedPtr<MCString>& name)
    : PTExpr(memctx, loc, ExprOperator::kBindVar),
      pos_(kUnsetPosition),
      name_(name) {
}

PTBindVar::PTBindVar(MemoryContext *memctx,
                     YBLocation::SharedPtr loc,
                     int64_t pos)
    : PTExpr(memctx, loc, ExprOperator::kBindVar),
      pos_(pos),
      name_(nullptr) {
}

PTBindVar::~PTBindVar() {
}

CHECKED_STATUS PTBindVar::Analyze(SemContext *sem_context) {
  RETURN_NOT_OK(CheckOperator(sem_context));

  if (name_ == nullptr) {
    name_ = sem_context->bindvar_name();
  }

  yql_type_ = sem_context->expr_expected_yql_type();
  internal_type_ = sem_context->expr_expected_internal_type();
  expected_internal_type_ = internal_type_;
  hash_col_ = sem_context->hash_col();
  if (hash_col_ != nullptr) {
    DCHECK(sem_context->current_dml_stmt() != nullptr);
    sem_context->current_dml_stmt()->AddHashColumnBindVar(this);
  }

  return Status::OK();
}

void PTBindVar::PrintSemanticAnalysisResult(SemContext *sem_context) {
  VLOG(3) << "SEMANTIC ANALYSIS RESULT (" << *loc_ << "):\n" << "Not yet avail";
}

}  // namespace sql
}  // namespace yb
