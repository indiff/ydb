#pragma once

namespace NYql {

extern int PG_FRAMEOPTION_NONDEFAULT;
extern int PG_FRAMEOPTION_RANGE;
extern int PG_FRAMEOPTION_ROWS;
extern int PG_FRAMEOPTION_GROUPS;
extern int PG_FRAMEOPTION_START_UNBOUNDED_PRECEDING;
extern int PG_FRAMEOPTION_START_OFFSET_PRECEDING;
extern int PG_FRAMEOPTION_START_CURRENT_ROW;
extern int PG_FRAMEOPTION_START_OFFSET_FOLLOWING;
extern int PG_FRAMEOPTION_START_UNBOUNDED_FOLLOWING;
extern int PG_FRAMEOPTION_END_UNBOUNDED_PRECEDING;
extern int PG_FRAMEOPTION_END_OFFSET_PRECEDING;
extern int PG_FRAMEOPTION_END_CURRENT_ROW;
extern int PG_FRAMEOPTION_END_OFFSET_FOLLOWING;
extern int PG_FRAMEOPTION_END_UNBOUNDED_FOLLOWING;
extern int PG_FRAMEOPTION_EXCLUDE_CURRENT_ROW;
extern int PG_FRAMEOPTION_EXCLUDE_GROUP;
extern int PG_FRAMEOPTION_EXCLUDE_TIES;

}
