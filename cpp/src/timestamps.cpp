/*
 * Copyright (c) 2023-2024, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>

// cudf's detail API of datetime assume that RMM has been included
#include <rmm/mr/device/device_memory_resource.hpp>

#include <cudf/detail/datetime.hpp>
#include <cudf/strings/convert/convert_datetime.hpp>

#include <legate_dataframe/core/column.hpp>
#include <legate_dataframe/core/library.hpp>
#include <legate_dataframe/core/task_argument.hpp>
#include <legate_dataframe/core/task_context.hpp>
#include <legate_dataframe/timestamps.hpp>

namespace legate::dataframe {
namespace task {

class ToTimestampsTask : public Task<ToTimestampsTask, OpCode::ToTimestamps> {
 public:
  static void gpu_variant(legate::TaskContext context)
  {
    GPUTaskContext ctx{context};
    const auto format = argument::get_next_scalar<std::string>(ctx);
    const auto input  = argument::get_next_input<PhysicalColumn>(ctx);
    auto output       = argument::get_next_output<PhysicalColumn>(ctx);

    std::unique_ptr<cudf::column> ret = cudf::strings::to_timestamps(
      input.column_view(), output.cudf_type(), format, ctx.stream(), ctx.mr());
    output.move_into(std::move(ret));
  }
};

class ExtractTimestampComponentTask
  : public Task<ExtractTimestampComponentTask, OpCode::ExtractTimestampComponent> {
 public:
  static void gpu_variant(legate::TaskContext context)
  {
    GPUTaskContext ctx{context};
    const auto component =
      argument::get_next_scalar<std::underlying_type_t<DatetimeComponent>>(ctx);
    const auto input = argument::get_next_input<PhysicalColumn>(ctx);
    auto output      = argument::get_next_output<PhysicalColumn>(ctx);

    std::unique_ptr<cudf::column> ret;
    /* unfortunately, there seems to be no templating for this in libcudf: */
    switch (static_cast<DatetimeComponent>(component)) {
      case DatetimeComponent::year:
        ret = cudf::datetime::detail::extract_year(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::month:
        ret = cudf::datetime::detail::extract_month(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::day:
        ret = cudf::datetime::detail::extract_day(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::weekday:
        ret = cudf::datetime::detail::extract_weekday(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::hour:
        ret = cudf::datetime::detail::extract_hour(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::minute:
        ret = cudf::datetime::detail::extract_minute(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::second:
        ret = cudf::datetime::detail::extract_second(input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::millisecond_fraction:
        ret = cudf::datetime::detail::extract_millisecond_fraction(
          input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::microsecond_fraction:
        ret = cudf::datetime::detail::extract_microsecond_fraction(
          input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::nanosecond_fraction:
        ret = cudf::datetime::detail::extract_nanosecond_fraction(
          input.column_view(), ctx.stream(), ctx.mr());
        break;
      case DatetimeComponent::day_of_year:
        ret = cudf::datetime::detail::day_of_year(input.column_view(), ctx.stream(), ctx.mr());
        break;
      default: throw std::runtime_error("invalid resolution to time part extraction?");
    }

    output.move_into(std::move(ret));
  }
};

}  // namespace task

LogicalColumn to_timestamps(const LogicalColumn& input,
                            cudf::data_type timestamp_type,
                            std::string format)
{
  auto runtime          = legate::Runtime::get_runtime();
  auto ret              = LogicalColumn::empty_like(timestamp_type, input.nullable());
  legate::AutoTask task = runtime->create_task(get_library(), task::ToTimestampsTask::TASK_ID);
  argument::add_next_scalar(task, std::move(format));
  argument::add_next_input(task, input);
  argument::add_next_output(task, ret);
  runtime->submit(std::move(task));
  return ret;
}

LogicalColumn extract_timestamp_component(const LogicalColumn& input, DatetimeComponent component)
{
  if (!cudf::is_timestamp(input.cudf_type())) {
    throw std::invalid_argument("extract_timestamp_component() input must be timestamp");
  }
  auto runtime = legate::Runtime::get_runtime();
  auto ret     = LogicalColumn::empty_like(cudf::data_type{cudf::type_id::INT16}, input.nullable());
  legate::AutoTask task =
    runtime->create_task(get_library(), task::ExtractTimestampComponentTask::TASK_ID);
  argument::add_next_scalar(task,
                            static_cast<std::underlying_type_t<DatetimeComponent>>(component));
  argument::add_next_input(task, input);
  argument::add_next_output(task, ret);
  runtime->submit(std::move(task));
  return ret;
}

}  // namespace legate::dataframe

namespace {

void __attribute__((constructor)) register_tasks()
{
  legate::dataframe::task::ToTimestampsTask::register_variants();
  legate::dataframe::task::ExtractTimestampComponentTask::register_variants();
}

}  // namespace
