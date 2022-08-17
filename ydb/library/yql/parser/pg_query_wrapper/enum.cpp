#include "enum.h"
#ifdef _WIN32
#define __restrict
#endif
extern "C" {
#include "postgres.h"
#include "nodes/parsenodes.h"
#undef Min
#undef Max
}

namespace NYql {

int PG_FRAMEOPTION_NONDEFAULT = FRAMEOPTION_NONDEFAULT;
int PG_FRAMEOPTION_RANGE = FRAMEOPTION_RANGE;
int PG_FRAMEOPTION_ROWS = FRAMEOPTION_ROWS;
int PG_FRAMEOPTION_GROUPS = FRAMEOPTION_GROUPS;
int PG_FRAMEOPTION_START_UNBOUNDED_PRECEDING = FRAMEOPTION_START_UNBOUNDED_PRECEDING;
int PG_FRAMEOPTION_START_OFFSET_PRECEDING = FRAMEOPTION_START_OFFSET_PRECEDING;
int PG_FRAMEOPTION_START_CURRENT_ROW = FRAMEOPTION_START_CURRENT_ROW;
int PG_FRAMEOPTION_START_OFFSET_FOLLOWING = FRAMEOPTION_START_OFFSET_FOLLOWING;
int PG_FRAMEOPTION_START_UNBOUNDED_FOLLOWING = FRAMEOPTION_START_UNBOUNDED_FOLLOWING;
int PG_FRAMEOPTION_END_UNBOUNDED_PRECEDING = FRAMEOPTION_END_UNBOUNDED_PRECEDING;
int PG_FRAMEOPTION_END_OFFSET_PRECEDING = FRAMEOPTION_END_OFFSET_PRECEDING;
int PG_FRAMEOPTION_END_CURRENT_ROW = FRAMEOPTION_END_CURRENT_ROW;
int PG_FRAMEOPTION_END_OFFSET_FOLLOWING = FRAMEOPTION_END_OFFSET_FOLLOWING;
int PG_FRAMEOPTION_END_UNBOUNDED_FOLLOWING = FRAMEOPTION_END_UNBOUNDED_FOLLOWING;
int PG_FRAMEOPTION_EXCLUDE_CURRENT_ROW = FRAMEOPTION_EXCLUDE_CURRENT_ROW;
int PG_FRAMEOPTION_EXCLUDE_GROUP = FRAMEOPTION_EXCLUDE_GROUP;
int PG_FRAMEOPTION_EXCLUDE_TIES = FRAMEOPTION_EXCLUDE_TIES;

}