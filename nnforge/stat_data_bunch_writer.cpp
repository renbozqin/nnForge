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

#include "stat_data_bunch_writer.h"

namespace nnforge
{
	stat_data_bunch_writer::stat_data_bunch_writer()
		: entry_count(0)
	{
	}

	stat_data_bunch_writer::~stat_data_bunch_writer()
	{
	}

	void stat_data_bunch_writer::set_config_map(const std::map<std::string, layer_configuration_specific> config_map)
	{
		layer_name_to_running_stat_list_map.clear();

		for(std::map<std::string, layer_configuration_specific>::const_iterator it = config_map.begin(); it != config_map.end(); ++it)
		{
			layer_name_to_running_stat_list_map.insert(std::make_pair(
				it->first,
				std::vector<running_stat>(it->second.feature_map_count)));
			layer_name_to_neuron_count_per_feature_map_map.insert(std::make_pair(
				it->first,
				it->second.get_neuron_count_per_feature_map()));
		}
	}

	void stat_data_bunch_writer::write(const std::map<std::string, const float *>& data_map)
	{
		for(std::map<std::string, const float *>::const_iterator it = data_map.begin(); it != data_map.end(); ++it)
		{
			const std::string& layer_name = it->first;
			const float * data = it->second;
			std::vector<running_stat>& running_stats = layer_name_to_running_stat_list_map.find(layer_name)->second;
			unsigned int neuron_count_per_feature_map = layer_name_to_neuron_count_per_feature_map_map[layer_name];
			unsigned int feature_map_count = static_cast<unsigned int>(running_stats.size());
			for(unsigned int feature_map_id = 0; feature_map_id < feature_map_count; ++feature_map_id)
			{
				running_stat current_running_stat;
				for(unsigned int i = 0; i < neuron_count_per_feature_map; ++i)
				{
					float val = *data;
					current_running_stat.min_val = std::min(current_running_stat.min_val, val);
					current_running_stat.max_val = std::max(current_running_stat.max_val, val);
					current_running_stat.sum += static_cast<double>(val);
					current_running_stat.sum_squared += static_cast<double>(val * val);

					++data;
				}

				{
					boost::lock_guard<boost::mutex> lock(update_stat_mutex);
					running_stats[feature_map_id].max_val = std::max(running_stats[feature_map_id].max_val, current_running_stat.max_val);
					running_stats[feature_map_id].min_val = std::min(running_stats[feature_map_id].min_val, current_running_stat.min_val);
					running_stats[feature_map_id].sum += current_running_stat.sum;
					running_stats[feature_map_id].sum_squared += current_running_stat.sum_squared;
				}
			}
		}

		++entry_count;
	}

	void stat_data_bunch_writer::write(
		unsigned int entry_id,
		const std::map<std::string, const float *>& data_map)
	{
		write(data_map); 
	}

	std::map<std::string, std::vector<feature_map_data_stat> > stat_data_bunch_writer::get_stat() const
	{
		std::map<std::string, std::vector<feature_map_data_stat> > res;

		for(std::map<std::string, std::vector<running_stat> >::const_iterator it = layer_name_to_running_stat_list_map.begin(); it != layer_name_to_running_stat_list_map.end(); ++it)
		{
			const std::string& layer_name = it->first;
			const std::vector<running_stat>& running_stats = it->second;
			unsigned int neuron_count_per_feature_map = layer_name_to_neuron_count_per_feature_map_map.find(layer_name)->second;
			double mult = 1.0 / ((double)entry_count * (double)neuron_count_per_feature_map);

			std::vector<feature_map_data_stat> new_stat_list;
			for(std::vector<running_stat>::const_iterator it2 = running_stats.begin(); it2 != running_stats.end(); ++it2)
			{
				feature_map_data_stat new_stat;

				const running_stat& current_running_stat = *it2;
				double avg = current_running_stat.sum * mult;
				new_stat.average = static_cast<float>(avg);
				double avg_sq = current_running_stat.sum_squared * mult;
				new_stat.std_dev = sqrtf(static_cast<float>(avg_sq - avg * avg));
				new_stat.min = current_running_stat.min_val;
				new_stat.max = current_running_stat.max_val;

				new_stat_list.push_back(new_stat);
			}

			res.insert(std::make_pair(layer_name, new_stat_list));
		}

		return res;
	}
}
