/*
 *  Copyright 2011-2015 Maxim Milakov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include "layer.h"

#include <map>
#include <string>

namespace nnforge
{
	class layer_factory
	{
	public:
		bool register_layer(layer::const_ptr sample_layer);

		bool unregister_layer(const std::string& layer_type_name);

		layer::ptr create_layer(const std::string& layer_type_name) const;

		unsigned int get_layer_type_id(const std::string& layer_type_name) const;

		static layer_factory& get_singleton();

	private:
		layer_factory() = default;

		std::map<std::string, layer::const_ptr> sample_name_layer_map;
		std::map<std::string, unsigned int> sample_name_id_map;
	};
}
