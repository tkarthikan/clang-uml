/**
 * tests/t30001/test_case.cc
 *
 * Copyright (c) 2021-2022 Bartek Kryza <bkryza@gmail.com>
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

TEST_CASE("t30001", "[test-case][package]")
{
    auto [config, db] = load_config("t30001");

    auto diagram = config.diagrams["t30001_package"];

    REQUIRE(diagram->should_include("clanguml::t30001::A"));
    REQUIRE(!diagram->should_include("clanguml::t30001::detail::C"));
    REQUIRE(!diagram->should_include("std::vector"));

    REQUIRE(diagram->name == "t30001_package");

    auto model = generate_package_diagram(db, diagram);

    REQUIRE(model.name() == "t30001_package");

    auto puml = generate_package_puml(diagram, model);
    AliasMatcher _A(puml);

    REQUIRE_THAT(puml, StartsWith("@startuml"));
    REQUIRE_THAT(puml, EndsWith("@enduml\n"));

    REQUIRE_THAT(puml, IsPackage("A"));
    REQUIRE_THAT(puml, IsPackage("AAA"));
    REQUIRE_THAT(puml, IsPackage("AAA"));

    save_puml(
        "./" + config.output_directory() + "/" + diagram->name + ".puml", puml);
}
