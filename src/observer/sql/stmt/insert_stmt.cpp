/* Copyright (c) 2021OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/insert_stmt.h"
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include<cstdio>
bool Leap_Year(int year){  //判断闰年
  if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return true;
    } else {
        return false;
    }
}
int32_t Int_to_Date(int orig_num){
  int OK_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};  //记录每个月合法天数
  int year=orig_num/10000;
  int month=(orig_num%10000)/100;
  int day=orig_num-year*10000-month*100;
  if((year>2038||year<1979) || (month>12 ||month <1)||day<1) return 0;
  else if(month!=2 && day>OK_days[month-1]) return 0;
  else if((month==2&&Leap_Year(year)&&day>29) ||(month==2&&!Leap_Year(year)&&day>28)) return 0;
  else return (year*10000+month*100+day);
}
int32_t String_to_Date(const std::string& orig_str){
  int year,month,day;
  sscanf(orig_str.c_str(),"%d-%d-%d",&year,&month,&day);
  return Int_to_Date(year*10000+month*100+day);
}
/*
2024-05-02
以上三个函数在values.cpp已定义，正确做法可能是要使用命名空间
课设为了节约时间，通过改函数名防止编译错误，以后再优化
*/

bool TypeConvert(const AttrType& should_be,const AttrType& current,Value& v_to_convert){  
  /*2024-05-02 By XinShaohan:目前先只支持字符串到日期的转换
    2024-05-04 By XinShaohan:新增支持整数到日期的转换
  */
  if(should_be==DATES&&current==CHARS){
    std::string orig_str=v_to_convert.get_string();
    int32_t date_conv=String_to_Date(orig_str);
    if(date_conv!=0){
      v_to_convert.set_date(date_conv);
      return true;
    }
    else return false;
  }
  else if(should_be==DATES&&current==INTS){
    int32_t date_conv=Int_to_Date(v_to_convert.get_int());
    if(date_conv!=0){
      v_to_convert.set_date(date_conv);
      return true;
    }
    else return false;
  }
  else return true; //NOP，以后补充
}

InsertStmt::InsertStmt(Table *table, const Value *values, int value_amount)
    : table_(table), values_(values), value_amount_(value_amount)
{}

RC InsertStmt::create(Db *db, InsertSqlNode &inserts, Stmt *&stmt)
{
  const char *table_name = inserts.relation_name.c_str();
  if (nullptr == db || nullptr == table_name || inserts.values.empty()) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, value_num=%d",
        db, table_name, static_cast<int>(inserts.values.size()));
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // check the fields number
  //InsertSqlNode inserts_can_modify=inserts;  //把inserts对象变成可修改的
  Value* values=inserts.values.data();
  const int value_num = static_cast<int>(inserts.values.size());
  const TableMeta &table_meta = table->table_meta();
  const int field_num = table_meta.field_num() - table_meta.sys_field_num();
  if (field_num != value_num) {
    LOG_WARN("schema mismatch. value num=%d, field num in schema=%d", value_num, field_num);
    return RC::SCHEMA_FIELD_MISSING;
  }

  // check fields type
  const int sys_field_num = table_meta.sys_field_num();
  for (int i = 0; i < value_num; i++) {
    const FieldMeta *field_meta = table_meta.field(i + sys_field_num);
    const AttrType field_type = field_meta->type();  //field_type是系统关于数据类型的信息（数据字典）
    const AttrType value_type = values[i].attr_type();  //value_type是插入数据的类型信息
    if (field_type != value_type) {  // TODO try to convert the value type to field type
      bool convert_res=TypeConvert(field_type,value_type,values[i]);
      if(!convert_res) return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }

  // everything alright
  stmt = new InsertStmt(table, values, value_num);
  return RC::SUCCESS;
}
