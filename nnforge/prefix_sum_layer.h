/*
 *  Copyright 2011-2016 Maxim Milakov
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

#include <vector>
#include <limits>

namespace nnforge
{
	class prefix_sum_layer : public layer
	{
	public:
		prefix_sum_layer(
			unsigned int feature_map_segment_length,
			float clamp_min = -std::numeric_limits<float>::max(),
			float clamp_max = std::numeric_limits<float>::max());

		virtual layer::ptr clone() const;

		virtual layer_configuration get_layer_configuration(const std::vector<layer_configuration>& input_configuration_list) const;

		virtual layer_configuration_specific get_output_layer_configuration_specific(const std::vector<layer_configuration_specific>& input_configuration_specific_list) const;

		virtual bool get_input_layer_configuration_specific(
			layer_configuration_specific& input_configuration_specific,
			const layer_configuration_specific& output_configuration_specific,
			unsigned int input_layer_id) const;

		virtual float get_flops_per_entry(
			const std::vector<layer_configuration_specific>& input_configuration_specific_list,
			const layer_action& action) const;

		virtual std::string get_type_name() const;

		virtual void write_proto(void * layer_proto) const;

		virtual void read_proto(const void * layer_proto);

		virtual std::vector<std::string> get_parameter_strings() const;

		static const std::string layer_type_name;

	private:
		void check();

	public:
		unsigned int feature_map_segment_length;
		float clamp_min;
		float clamp_max;
	};
}
