/**
 * tests/t00018/test_case.h
 *
 * Copyright (c) 2021-2025 Bartek Kryza <bkryza@gmail.com>
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

TEST_CASE("t00018")
{
    using namespace clanguml::test;

    auto [config, db, diagram, model] =
        CHECK_CLASS_MODEL("t00018", "t00018_class");

    CHECK_CLASS_DIAGRAM(*config, diagram, *model, [](const auto &src) {
        REQUIRE(IsClass(src, "widget"));
        REQUIRE(IsClass(src, "impl::widget"));
        REQUIRE(IsDependency(src, "impl::widget", "widget"));

        REQUIRE(IsAggregation<Private>(src, "widget", "impl::widget", "pImpl"));
        REQUIRE(IsDependency(src, "impl::widget", "widget"));
        REQUIRE(!IsDependency(src, "widget", "widget"));
    });
}