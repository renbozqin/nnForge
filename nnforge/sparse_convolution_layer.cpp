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

#include "sparse_convolution_layer.h"

#include "neural_network_exception.h"
#include "proto/nnforge.pb.h"

#include <algorithm>
#include <set>
#include <boost/format.hpp>
#include <sstream>
#include <numeric>

namespace nnforge
{
	const std::string sparse_convolution_layer::layer_type_name = "SparseConvolution";

	sparse_convolution_layer::sparse_convolution_layer(
		const std::vector<unsigned int>& window_sizes,
		unsigned int input_feature_map_count,
		unsigned int output_feature_map_count,
		unsigned int feature_map_connection_count,
		const std::vector<unsigned int>& left_zero_padding,
		const std::vector<unsigned int>& right_zero_padding,
		const std::vector<unsigned int>& strides,
		bool bias)
		: window_sizes(window_sizes),
		input_feature_map_count(input_feature_map_count),
		output_feature_map_count(output_feature_map_count),
		feature_map_connection_sparsity_ratio(-1.0F),
		feature_map_connection_count(feature_map_connection_count),
		bias(bias)
	{
		if ((left_zero_padding.size() != 0) && (left_zero_padding.size() != window_sizes.size()))
			throw std::runtime_error((boost::format("Invalid dimension count %1% for left zero padding") % left_zero_padding.size()).str());
		if ((right_zero_padding.size() != 0) && (right_zero_padding.size() != window_sizes.size()))
			throw std::runtime_error((boost::format("Invalid dimension count %1% for right zero padding") % right_zero_padding.size()).str());
		if ((strides.size() != 0) && (strides.size() != window_sizes.size()))
			throw std::runtime_error((boost::format("Invalid dimension count %1% for strides") % strides.size()).str());

		if (left_zero_padding.empty())
			this->left_zero_padding.resize(window_sizes.size(), 0);
		else
			this->left_zero_padding = left_zero_padding;

		if (right_zero_padding.empty())
			this->right_zero_padding.resize(window_sizes.size(), 0);
		else
			this->right_zero_padding = right_zero_padding;

		if (strides.empty())
			this->strides.resize(window_sizes.size(), 1);
		else
			this->strides = strides;

		check();
	}

	sparse_convolution_layer::sparse_convolution_layer(
		const std::vector<unsigned int>& window_sizes,
		unsigned int input_feature_map_count,
		unsigned int output_feature_map_count,
		float feature_map_connection_sparsity_ratio,
		const std::vector<unsigned int>& left_zero_padding,
		const std::vector<unsigned int>& right_zero_padding,
		const std::vector<unsigned int>& strides,
		bool bias)
		: window_sizes(window_sizes),
		input_feature_map_count(input_feature_map_count),
		output_feature_map_count(output_feature_map_count),
		feature_map_connection_sparsity_ratio(feature_map_connection_sparsity_ratio),
		feature_map_connection_count(static_cast<unsigned int>(input_feature_map_count * output_feature_map_count * feature_map_connection_sparsity_ratio + 0.5F)),
		bias(bias)
	{
		if ((left_zero_padding.size() != 0) && (left_zero_padding.size() != window_sizes.size()))
			throw std::runtime_error((boost::format("Invalid dimension count %1% for left zero padding") % left_zero_padding.size()).str());
		if ((right_zero_padding.size() != 0) && (right_zero_padding.size() != window_sizes.size()))
			throw std::runtime_error((boost::format("Invalid dimension count %1% for right zero padding") % right_zero_padding.size()).str());
		if ((strides.size() != 0) && (strides.size() != window_sizes.size()))
			throw std::runtime_error((boost::format("Invalid dimension count %1% for strides") % strides.size()).str());

		if (left_zero_padding.empty())
			this->left_zero_padding.resize(window_sizes.size(), 0);
		else
			this->left_zero_padding = left_zero_padding;

		if (right_zero_padding.empty())
			this->right_zero_padding.resize(window_sizes.size(), 0);
		else
			this->right_zero_padding = right_zero_padding;

		if (strides.empty())
			this->strides.resize(window_sizes.size(), 1);
		else
			this->strides = strides;

		check();
	}

	void sparse_convolution_layer::check()
	{
		for(unsigned int i = 0; i < window_sizes.size(); i++)
			if (window_sizes[i] == 0)
				throw neural_network_exception("window dimension for sparse convolution layer may not be zero");

		if (feature_map_connection_count < input_feature_map_count)
			throw neural_network_exception("feature_map_connection_count may not be smaller than input_feature_map_count");
		if (feature_map_connection_count < output_feature_map_count)
			throw neural_network_exception("feature_map_connection_count may not be smaller than output_feature_map_count");
		if (feature_map_connection_count > input_feature_map_count * output_feature_map_count)
			throw neural_network_exception("feature_map_connection_count may not be larger than in dense case");

		for(unsigned int i = 0; i < window_sizes.size(); i++)
			if (left_zero_padding[i] >= window_sizes[i])
				throw neural_network_exception((boost::format("left zero padding %1% of dimension (%2%) is greater or equal than layer window size (%3%)") % left_zero_padding[i] % i % window_sizes[i]).str());

		for(unsigned int i = 0; i < window_sizes.size(); i++)
			if (right_zero_padding[i] >= window_sizes[i])
				throw neural_network_exception((boost::format("right zero padding %1% of dimension (%2%) is greater or equal than layer window size (%3%)") % right_zero_padding[i] % i % window_sizes[i]).str());

		for(unsigned int i = 0; i < strides.size(); i++)
			if (strides[i] == 0)
				throw neural_network_exception((boost::format("stride dimension (%1%) is 0") % i).str());
	}

	std::string sparse_convolution_layer::get_type_name() const
	{
		return layer_type_name;
	}

	layer::ptr sparse_convolution_layer::clone() const
	{
		return layer::ptr(new sparse_convolution_layer(*this));
	}

	layer_configuration_specific sparse_convolution_layer::get_output_layer_configuration_specific(const std::vector<layer_configuration_specific>& input_configuration_specific_list) const
	{
		if (input_configuration_specific_list[0].feature_map_count != input_feature_map_count)
			throw neural_network_exception((boost::format("Feature map count in layer (%1%) and input configuration (%2%) don't match") % input_feature_map_count % input_configuration_specific_list[0].feature_map_count).str());

		if (input_configuration_specific_list[0].get_dimension_count() != window_sizes.size())
			throw neural_network_exception((boost::format("Dimension count in layer (%1%) and input configuration (%2%) don't match") % window_sizes.size() % input_configuration_specific_list[0].get_dimension_count()).str());

		layer_configuration_specific res(output_feature_map_count);

		for(unsigned int i = 0; i < window_sizes.size(); ++i)
		{
			unsigned int total_input_dimension_size = input_configuration_specific_list[0].dimension_sizes[i] + left_zero_padding[i] + right_zero_padding[i];

			if (total_input_dimension_size < window_sizes[i])
				throw neural_network_exception((boost::format("Too small total dimension size (with padding) (%1%) of dimension (%2%) is smaller than layer window size (%3%)") % total_input_dimension_size % i % window_sizes[i]).str());

			res.dimension_sizes.push_back((total_input_dimension_size - window_sizes[i]) / strides[i] + 1);
		}

		return res;
	}

	bool sparse_convolution_layer::get_input_layer_configuration_specific(
		layer_configuration_specific& input_configuration_specific,
		const layer_configuration_specific& output_configuration_specific,
		unsigned int input_layer_id) const
	{
		if (output_configuration_specific.feature_map_count != output_feature_map_count)
			throw neural_network_exception((boost::format("Feature map count in layer (%1%) and output configuration (%2%) don't match") % output_feature_map_count % output_configuration_specific.feature_map_count).str());

		if (output_configuration_specific.get_dimension_count() != window_sizes.size())
			throw neural_network_exception((boost::format("Dimension count in layer (%1%) and input configuration (%2%) don't match") % window_sizes.size() % output_configuration_specific.get_dimension_count()).str());

		input_configuration_specific = layer_configuration_specific(output_feature_map_count);

		for(unsigned int i = 0; i < window_sizes.size(); ++i)
			input_configuration_specific.dimension_sizes.push_back((output_configuration_specific.dimension_sizes[i] - 1) * strides[i] + window_sizes[i] - left_zero_padding[i] - right_zero_padding[i]);

		return true;
	}

	void sparse_convolution_layer::write_proto(void * layer_proto) const
	{
		protobuf::Layer * layer_proto_typed = reinterpret_cast<protobuf::Layer *>(layer_proto);
		protobuf::SparseConvolutionalParam * param = layer_proto_typed->mutable_sparse_convolution_param();

		param->set_output_feature_map_count(output_feature_map_count);
		param->set_input_feature_map_count(input_feature_map_count);
		if (!bias)
			param->set_bias(false);

		if (feature_map_connection_sparsity_ratio >= 0.0F)
			param->set_feature_map_connection_sparsity_ratio(feature_map_connection_sparsity_ratio);
		else
			param->set_feature_map_connection_count(feature_map_connection_count);

		for(int i = 0; i < window_sizes.size(); ++i)
		{
			protobuf::SparseConvolutionalParam_SparseConvolutionalDimensionParam * dim_param = param->add_dimension_param();
			dim_param->set_kernel_size(window_sizes[i]);
			if (left_zero_padding[i] > 0)
				dim_param->set_left_padding(left_zero_padding[i]);
			if (right_zero_padding[i] > 0)
				dim_param->set_right_padding(right_zero_padding[i]);
			if (strides[i] > 1)
				dim_param->set_stride(strides[i]);
		}
	}

	void sparse_convolution_layer::read_proto(const void * layer_proto)
	{
		const protobuf::Layer * layer_proto_typed = reinterpret_cast<const protobuf::Layer *>(layer_proto);
		if (!layer_proto_typed->has_sparse_convolution_param())
			throw neural_network_exception((boost::format("No sparse_convolution_param specified for layer %1% of type %2%") % instance_name % layer_proto_typed->type()).str());

		const nnforge::protobuf::SparseConvolutionalParam& param = layer_proto_typed->sparse_convolution_param();

		input_feature_map_count = param.input_feature_map_count();
		output_feature_map_count = param.output_feature_map_count();
		bias = param.bias();

		window_sizes.resize(param.dimension_param_size());
		left_zero_padding.resize(param.dimension_param_size());
		right_zero_padding.resize(param.dimension_param_size());
		strides.resize(param.dimension_param_size());

		for(int i = 0; i < param.dimension_param_size(); ++i)
		{
			window_sizes[i] = param.dimension_param(i).kernel_size();
			left_zero_padding[i] = param.dimension_param(i).left_padding();
			right_zero_padding[i] = param.dimension_param(i).right_padding();
			strides[i] = param.dimension_param(i).stride();
		}

		if (param.has_feature_map_connection_count())
		{
			feature_map_connection_sparsity_ratio = -1.0F;
			feature_map_connection_count = param.feature_map_connection_count();
		}
		else if (param.has_feature_map_connection_sparsity_ratio())
		{
			feature_map_connection_sparsity_ratio = param.feature_map_connection_sparsity_ratio();
			feature_map_connection_count = static_cast<unsigned int>(input_feature_map_count * output_feature_map_count * feature_map_connection_sparsity_ratio + 0.5F);
		}
		else
			throw neural_network_exception((boost::format("No sparsity pattern defined in sparse_convolution_param specified for layer %1% of type %2%") % instance_name % layer_proto_typed->type()).str());

		check();
	}

	data_config sparse_convolution_layer::get_data_config() const
	{
		data_config res;

		unsigned int weight_count = feature_map_connection_count;
		std::for_each(window_sizes.begin(), window_sizes.end(), [&weight_count] (unsigned int x) { weight_count *= x; });

		res.push_back(weight_count);
		if (bias)
			res.push_back(output_feature_map_count);

		return res;
	}

	data_custom_config sparse_convolution_layer::get_data_custom_config() const
	{
		data_custom_config res;

		res.push_back(feature_map_connection_count); // column indices
		res.push_back(output_feature_map_count + 1); // row indices

		return res;
	}

	void sparse_convolution_layer::randomize_data(
		layer_data::ptr data,
		layer_data_custom::ptr data_custom,
		random_generator& generator) const
	{
		randomize_custom_data(*data_custom, generator);

		randomize_weights(*data, *data_custom, generator);
	}

	void sparse_convolution_layer::randomize_weights(
		layer_data& data,
		const layer_data_custom& data_custom,
		random_generator& generator) const
	{
		unsigned int weight_count = 1;
		std::for_each(window_sizes.begin(), window_sizes.end(), [&weight_count] (unsigned int x) { weight_count *= x; });

		unsigned int current_weight_index = 0;
		for(unsigned int output_feature_map_id = 0; output_feature_map_id < output_feature_map_count; ++output_feature_map_id)
		{
			unsigned int current_input_feature_map_count = data_custom[1][output_feature_map_id + 1] - data_custom[1][output_feature_map_id];
			if (current_input_feature_map_count > 0)
			{
				float current_average_feature_map_count = sqrtf(static_cast<float>(current_input_feature_map_count) * static_cast<float>(output_feature_map_count));
				float standard_deviation = sqrtf(1.0F / (current_average_feature_map_count * static_cast<float>(weight_count)));
				float max_abs_value = 100.0F * standard_deviation;
				std::normal_distribution<float> nd(0.0F, standard_deviation);

				unsigned int currrent_input_neuron_count = weight_count * current_input_feature_map_count;
				for(unsigned int i = 0; i < currrent_input_neuron_count; ++i)
				{
					float val = nd(generator);
					while (fabs(val) > max_abs_value)
						val = nd(generator);

					data[0][current_weight_index] = val;
					++current_weight_index;
				}
			}
		}

		if (bias)
			std::fill(data[1].begin(), data[1].end(), 0.0F);
	}

	void sparse_convolution_layer::fill_connection_matrix(
		std::vector<bool>& connection_matrix,
		random_generator& generator,
		unsigned int output_feature_map_count,
		unsigned int input_feature_map_count,
		unsigned int feature_map_connection_count)
	{
		unsigned int margin = 0;
		while(true)
		{
			std::vector<bool> local_connection_matrix = connection_matrix;
			unsigned int max_connections_for_output_feature_map = (feature_map_connection_count + output_feature_map_count - 1) / output_feature_map_count + margin;
			unsigned int max_connections_for_input_feature_map = (feature_map_connection_count + input_feature_map_count - 1) / input_feature_map_count + margin;
			unsigned int max_connections_for_output_feature_map_overflow = std::max(max_connections_for_output_feature_map + 1, static_cast<unsigned int>(max_connections_for_output_feature_map * 1.01F));
			unsigned int max_connections_for_input_feature_map_overflow = std::max(max_connections_for_input_feature_map + 1, static_cast<unsigned int>(max_connections_for_input_feature_map * 1.01F));

			std::vector<unsigned int> output_feature_map_connection_count_list(output_feature_map_count);
			for(unsigned int output_feature_map_id = 0; output_feature_map_id < output_feature_map_count; ++output_feature_map_id)
			{
				unsigned int feature_map_count = 0;
				for(unsigned int input_feature_map_id = 0; input_feature_map_id < input_feature_map_count; ++input_feature_map_id)
					if (local_connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id])
						++feature_map_count;
				output_feature_map_connection_count_list[output_feature_map_id] = feature_map_count;
			}
			std::vector<unsigned int> output_feature_map_id_available_list;
			std::vector<unsigned int> output_feature_map_id_available_overflow_list;
			for(unsigned int i = 0; i < output_feature_map_count; ++i)
			{
				if (output_feature_map_connection_count_list[i] < max_connections_for_output_feature_map)
					output_feature_map_id_available_list.push_back(i);
				if (output_feature_map_connection_count_list[i] < max_connections_for_output_feature_map_overflow)
					output_feature_map_id_available_overflow_list.push_back(i);
			}

			std::vector<unsigned int> input_feature_map_connection_count_list(input_feature_map_count);
			for(unsigned int input_feature_map_id = 0; input_feature_map_id < input_feature_map_count; ++input_feature_map_id)
			{
				unsigned int feature_map_count = 0;
				for(unsigned int output_feature_map_id = 0; output_feature_map_id < output_feature_map_count; ++output_feature_map_id)
					if (local_connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id])
						++feature_map_count;
				input_feature_map_connection_count_list[input_feature_map_id] = feature_map_count;
			}
			std::vector<unsigned int> input_feature_map_id_available_list;
			std::vector<unsigned int> input_feature_map_id_available_overflow_list;
			for(unsigned int i = 0; i < input_feature_map_count; ++i)
			{
				if (input_feature_map_connection_count_list[i] < max_connections_for_input_feature_map)
					input_feature_map_id_available_list.push_back(i);
				if (input_feature_map_connection_count_list[i] < max_connections_for_input_feature_map_overflow)
					input_feature_map_id_available_overflow_list.push_back(i);
			}

			bool explore_strict = true;
			bool should_retry_with_larger_margin = false;
			unsigned int current_output_feature_map_index = 0;
			int initial_connection_count = std::accumulate(output_feature_map_connection_count_list.begin(), output_feature_map_connection_count_list.end(), 0);
			for(int i = initial_connection_count; i < static_cast<int>(feature_map_connection_count); ++i)
			{
				unsigned int output_feature_map_id;
				unsigned int input_feature_map_id;
				bool found = false;
				if (explore_strict)
				{
					for(int attempt_id = 0; (attempt_id < 20) && (!found); ++attempt_id)
					{
						output_feature_map_id = output_feature_map_id_available_list[current_output_feature_map_index];
						std::uniform_int_distribution<unsigned int> in_dist(0U, static_cast<unsigned int>(input_feature_map_id_available_list.size() - 1));
						input_feature_map_id = input_feature_map_id_available_list[in_dist(generator)];
						found = !local_connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id];
					}
					if (!found)
					{
						for(int attempt_id = 0; (attempt_id < 100) && (!found); ++attempt_id)
						{
							std::uniform_int_distribution<unsigned int> out_dist(0U, static_cast<unsigned int>(output_feature_map_id_available_list.size() - 1));
							output_feature_map_id = output_feature_map_id_available_list[out_dist(generator)];
							std::uniform_int_distribution<unsigned int> in_dist(0U, static_cast<unsigned int>(input_feature_map_id_available_list.size() - 1));
							input_feature_map_id = input_feature_map_id_available_list[in_dist(generator)];
							found = !local_connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id];
						}
					}
					explore_strict = found;
				}

				if (!found)
				{
					for(int attempt_id = 0; (attempt_id < 100) && (!found); ++attempt_id)
					{
						std::uniform_int_distribution<unsigned int> out_dist(0U, static_cast<unsigned int>(output_feature_map_id_available_overflow_list.size() - 1));
						output_feature_map_id = output_feature_map_id_available_overflow_list[out_dist(generator)];
						std::uniform_int_distribution<unsigned int> in_dist(0U, static_cast<unsigned int>(input_feature_map_id_available_overflow_list.size() - 1));
						input_feature_map_id = input_feature_map_id_available_overflow_list[in_dist(generator)];
						found = !local_connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id];
					}
				}

				if (found)
				{
					local_connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id] = true;

					unsigned int output_feature_map_connection_count = ++output_feature_map_connection_count_list[output_feature_map_id];
					if (output_feature_map_connection_count == max_connections_for_output_feature_map)
						output_feature_map_id_available_list.erase(std::find(output_feature_map_id_available_list.begin(), output_feature_map_id_available_list.end(), output_feature_map_id));
					else
					{
						++current_output_feature_map_index;
						if (output_feature_map_connection_count == max_connections_for_output_feature_map_overflow)
							output_feature_map_id_available_overflow_list.erase(std::find(output_feature_map_id_available_overflow_list.begin(), output_feature_map_id_available_overflow_list.end(), output_feature_map_id));
					}

					unsigned int input_feature_map_connection_count = ++input_feature_map_connection_count_list[input_feature_map_id];
					if (input_feature_map_connection_count == max_connections_for_input_feature_map)
						input_feature_map_id_available_list.erase(std::find(input_feature_map_id_available_list.begin(), input_feature_map_id_available_list.end(), input_feature_map_id));
					else if (input_feature_map_connection_count == max_connections_for_input_feature_map_overflow)
						input_feature_map_id_available_overflow_list.erase(std::find(input_feature_map_id_available_overflow_list.begin(), input_feature_map_id_available_overflow_list.end(), input_feature_map_id));

					if (!output_feature_map_id_available_list.empty())
						current_output_feature_map_index = current_output_feature_map_index % output_feature_map_id_available_list.size();

					if (explore_strict)
						explore_strict = (!output_feature_map_id_available_list.empty()) && (!input_feature_map_id_available_list.empty());
				}
				else
				{
					should_retry_with_larger_margin = true;
					break;
				}
			}


			if (!should_retry_with_larger_margin)
			{
				connection_matrix = local_connection_matrix;
				break;
			}

			++margin;
		}
	}

	void sparse_convolution_layer::fill_data_custom(
		layer_data_custom& data_custom,
		const std::vector<bool>& connection_matrix,
		unsigned int output_feature_map_count,
		unsigned int input_feature_map_count)
	{
		int current_column_offset = 0;
		for(int output_feature_map_id = 0; output_feature_map_id < static_cast<int>(output_feature_map_count); ++output_feature_map_id)
		{
			data_custom[1][output_feature_map_id] = current_column_offset;

			for(int input_feature_map_id = 0; input_feature_map_id < static_cast<int>(input_feature_map_count); ++input_feature_map_id)
			{
				if (connection_matrix[output_feature_map_id * input_feature_map_count + input_feature_map_id])
				{
					data_custom[0][current_column_offset] = input_feature_map_id;
					++current_column_offset;
				}
			}
		}
		data_custom[1][output_feature_map_count] = current_column_offset;
	}

	void sparse_convolution_layer::randomize_custom_data(
		layer_data_custom& data_custom,
		random_generator& generator) const
	{
		std::vector<bool> connection_matrix(output_feature_map_count * input_feature_map_count);

		fill_connection_matrix(
			connection_matrix,
			generator,
			output_feature_map_count,
			input_feature_map_count,
			feature_map_connection_count);

		fill_data_custom(
			data_custom,
			connection_matrix,
			output_feature_map_count,
			input_feature_map_count);
	}

	float sparse_convolution_layer::get_flops_per_entry(
		const std::vector<layer_configuration_specific>& input_configuration_specific_list,
		const layer_action& action) const
	{
		switch (action.get_action_type())
		{
		case layer_action::forward:
		case layer_action::backward_data:
		case layer_action::backward_weights:
			{
				unsigned int neuron_count = get_output_layer_configuration_specific(input_configuration_specific_list).get_neuron_count_per_feature_map();
				unsigned int per_item_flops = feature_map_connection_count * 2;
				std::for_each(window_sizes.begin(), window_sizes.end(), [&per_item_flops] (unsigned int x) { per_item_flops *= x; });
				if (!bias)
					--per_item_flops;
				return static_cast<float>(neuron_count) * static_cast<float>(per_item_flops);
			}
		default:
			return 0.0F;
		}
	}

	layer_data_configuration_list sparse_convolution_layer::get_layer_data_configuration_list() const
	{
		layer_data_configuration_list res;
		res.push_back(layer_data_configuration(1, feature_map_connection_count, window_sizes));
		if (bias)
			res.push_back(layer_data_configuration(1, output_feature_map_count, std::vector<unsigned int>()));

		return res;
	}

	std::set<unsigned int> sparse_convolution_layer::get_weight_decay_part_id_set() const
	{
		std::set<unsigned int> res;
		res.insert(0);
		return res;
	}

	std::vector<std::string> sparse_convolution_layer::get_parameter_strings() const
	{
		std::vector<std::string> res;

		{
			std::stringstream ss;
			if (window_sizes.empty())
			{
				ss << "fc";
			}
			else
			{
				for(int i = 0; i < window_sizes.size(); ++i)
				{
					if (i != 0)
						ss << "x";
					ss << window_sizes[i];
				}
			}
			ss << ", fm " << input_feature_map_count << "x" << output_feature_map_count;

			bool empty_padding = true;
			for(int i = 0; i < left_zero_padding.size(); ++i)
			{
				if ((left_zero_padding[i] != 0) || (right_zero_padding[i] != 0))
				{
					empty_padding = false;
					break;
				}
			}
			if (!empty_padding)
			{
				ss << ", pad ";
				for(int i = 0; i < left_zero_padding.size(); ++i)
				{
					if (i != 0)
						ss << "x";
					if (left_zero_padding[i] == right_zero_padding[i])
						ss << left_zero_padding[i];
					else
						ss << left_zero_padding[i] << "_" << right_zero_padding[i];
				}
			}

			bool empty_stride = true;
			for(int i = 0; i < strides.size(); ++i)
			{
				if (strides[i] != 1)
				{
					empty_stride = false;
					break;
				}
			}
			if (!empty_stride)
			{
				ss << ", stride ";
				for(int i = 0; i < strides.size(); ++i)
				{
					if (i != 0)
						ss << "x";
					ss << strides[i];
				}
			}
			if (!bias)
				ss << ", w/out bias";

			res.push_back(ss.str());
		}

		{
			std::stringstream ss;
			if (feature_map_connection_sparsity_ratio >= 0.0F)
				ss << (boost::format("sparsity ratio %|1$.5f|") % feature_map_connection_sparsity_ratio).str();
			else
				ss << "connections " << feature_map_connection_count;

			res.push_back(ss.str());
		}

		return res;
	}
}
