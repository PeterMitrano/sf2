/* ============================================
 SF2 source code is placed under the MIT license
 Copyright (c) 2017 Kauai Labs

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ===============================================
 */

#ifndef SRC_TIME_THREADSAFEINTERPOLATINGTIMEHISTORY_H_
#define SRC_TIME_THREADSAFEINTERPOLATINGTIMEHISTORY_H_

/**
 * The ThreadsafeInterpolatingTimeHistory class implements an array of
 * timestamped objects which can be inserted from one thread and accessed
 * by another thread.  The accessing thread can lookup objects within the
 * ThreadsafeInterpolatingTimeHistory based upon a timesatamp, and in
 * cases where an exact timestamp match is not found, and object with
 * interpolated values is returned.
 * <p>
 * This class is a template class, meaning that it can be used to contain
 * any type of object which implements the ITimestampedValue and
 * IValueInterpolator interfaces.
 * <p>
 * The implementation of this class is such that the contained objects
 * are statically allocated to avoid memory allocation when objects are
 * added.
 * @author Scott
 */

#include <forward_list>
#include <vector>
#include <string>
#include <climits>
#include <regex>
#include <mutex>
#include "../unit/Unit.h"
#include "../platform/File.h"
#include "TimestampInfo.h"
using namespace std;

class CustomFilenameFilter: public FilenameFilter {
	string& prefix;
	string& suffix;
public:
	CustomFilenameFilter(string& prefix, string& suffix) :
		prefix(prefix),
		suffix(suffix){
	}
	bool accept(File& dir, const string& name) {
		bool starts_with = (name.find(prefix) == 0);
		bool ends_with = (name.find(suffix)
				== name.size() - suffix.size() - 1);
		return starts_with && ends_with;
	}
	~CustomFilenameFilter(){}
};

template<typename T>
class ThreadsafeInterpolatingTimeHistory {
	TimestampInfo ts_info;
	mutex list_mutex;
	vector<T *> history;
	int history_size;
	int curr_index;
	int num_valid_samples;
	T default_obj;
	string value_name;

public:
	/**
	 * Constructs a ThreadsafeInterpolatingTimeHihstory to hold up to a specified number of
	 * objects of the specified class.
	 * @param _class - the Java class of the objects to be contained.
	 * @param num_samples - the maximum number of objects to be contained.
	 */

	ThreadsafeInterpolatingTimeHistory(T& default_obj, int num_samples,
			TimestampInfo& ts_info, string& name) :
			ts_info(ts_info),
			list_mutex() {
		history_size = num_samples;

		for ( int i = 0; i < num_samples; i++) {
			T *p_t = new T();
			history.push_back(p_t);
		}

		this->default_obj = default_obj;
		curr_index = 0;
		num_valid_samples = 0;
		this->value_name = name;
	}

	~ThreadsafeInterpolatingTimeHistory()
	{
		for ( size_t i = 0; i < history.size(); i++) {
			delete history[i];
			history[i] = NULL;
		}
	}

	/**
	 * Clears all contents of the ThreadsafeInterpolatingTimeHistory by marking all
	 * contained objects as invalid.
	 */
	void reset() {
		std::unique_lock<mutex> sync(list_mutex);
		for (int i = 0; i < history_size; i++) {
			T* p_t = history[i];
			p_t->setValid(false);
		}
		curr_index = 0;
		num_valid_samples = 0;
	}

	/**
	 * Returns the current count of valid objects in this ThreadsafeInterpolatingTimeHistory.
	 * @return
	 */
	int getValidSampleCount() {
		return num_valid_samples;
	}

	/**
	 * Adds the provided object to the ThreadsafeInterpolatingTimeHistory.
	 * @param t - the object to add
	 */
	void add(T& t) {
		std::unique_lock<mutex> sync(list_mutex);
		T* p_existing = history[curr_index];
		p_existing->copy(t);
		curr_index++;
		if (curr_index >= history_size) {
			curr_index = 0;
		}
		if (num_valid_samples < history_size) {
			num_valid_samples++;
		}
	}

	/**
	 * Retrieves the object in the ThreadsafeInterpolatingTimeHistory which matches
	 * the provided timestamp.  If an exact match is not found, a new object will be
	 * created using interpolated values, based upon the nearest objects preceding and
	 * following the requested timestamp.
	 * @param requested_timestamp - the timeatamp for which to return an object
	 * @return - returns the object (either actual or interpolated) matching the requested
	 * timestamp.  If no object could be located or interpolated, null is returned.
	 */
	bool get(long requested_timestamp, T& out) {
		bool success = false;
		T* p_match = NULL;
		long nearest_preceding_timestamp = LONG_MIN;
		long nearest_preceding_timestamp_delta = LONG_MIN;
		T *p_nearest_preceding_obj = NULL;
		long nearest_following_timestamp_delta = LONG_MAX;
		T *p_nearest_following_obj = NULL;
		bool copy_object = true;
		{
			std::unique_lock<mutex> sync(list_mutex);

			int entry_index = curr_index;
			for (int i = 0; i < num_valid_samples; i++) {
				T *p_obj = history[entry_index];
				long entry_timestamp = p_obj->getTimestamp();
				long delta = entry_timestamp - requested_timestamp;
				if (delta < 0) {
					if (delta > nearest_preceding_timestamp_delta) {
						nearest_preceding_timestamp_delta = delta;
						nearest_preceding_timestamp = entry_timestamp;
						p_nearest_preceding_obj = p_obj;
						/* To optimize, break out once both nearest preceding
						 * and following entries are found.  This optimization
						 * relies on entries being in descending timestamp
						 * order, beginning with the current entry.
						 */
						if (p_nearest_following_obj != NULL)
							break;
					}
				} else if (delta > 0) {
					if (delta < nearest_following_timestamp_delta) {
						nearest_following_timestamp_delta = delta;
						p_nearest_following_obj = p_obj;
					}
				} else { /* entry_timestamp == requested_timestamp */
					p_match = p_obj;
					break;
				}
				entry_index--;
				if (entry_index < 0) {
					entry_index = history_size - 1;
				}
			}

			/* If a match was not found, and the requested timestamp falls
			 * within two entries in the history, interpolate an intermediate
			 * value.
			 */
			if ((p_match == NULL) && (p_nearest_preceding_obj != NULL)
					&& (p_nearest_following_obj != NULL)) {
				double timestamp_delta = nearest_following_timestamp_delta
						- nearest_preceding_timestamp_delta;
				double requested_timestamp_offset = requested_timestamp
						- nearest_preceding_timestamp;
				double requested_timestamp_ratio = requested_timestamp_offset
						/ timestamp_delta;

				p_nearest_preceding_obj->interpolate(*p_nearest_following_obj,
						requested_timestamp_ratio, out);
				out.setInterpolated(true);
				copy_object = false;
				success = true;
			}

			if ((p_match != NULL) && copy_object) {
				/* Make a copy of the object, so that caller does not directly reference
				 * an object within the volatile (threadsafe) history.
				 */
				out.copy(*p_match);
				out.setInterpolated(false);
				success = true;
			}
		}

		return success;
	}

	/**
	 * Retrieves the most recently-added object in the ThreadsafeInterpolatingTimeHistory.
	 * @return - the most recently-added object, or null if no valid objects exist
	 */
	bool getMostRecent(T& out) {
		T* p_most_recent_t = NULL;
		{
			std::unique_lock<mutex> sync(list_mutex);

			if (num_valid_samples > 0) {
				int curr_idx = this->curr_index;
				curr_idx--;
				if (curr_idx < 0) {
					curr_idx = (history_size - 1);
				}
				p_most_recent_t = history[curr_idx];
				if (!p_most_recent_t->getValid()) {
					p_most_recent_t = NULL;
				}
			}
		}
		if (p_most_recent_t != NULL) {
			/* Make a copy of the object, so that caller does not directly
			 * reference an object within the volatile (threadsafe) history. */
			out.copy(*p_most_recent_t);
			return true;
		} else {
			return false;
		}
	}

	bool writeToDirectory(string& directory) {
		File dir(directory);
		if (!dir.isDirectory() || !dir.canWrite()) {
			printf("Directory parameter '%s' must be a writable directory.", directory.c_str());
			return false;
		}

		if ((directory[(directory.length() - 1)] != '/')
				&& (directory[(directory.length() - 1)] != '\\')) {
			directory += File::separatorChar;
		}

		string filename_prefix = value_name + "History";
		string filename_suffix = "csv";

		File f(directory);
		forward_list<File *> matching_files;
		CustomFilenameFilter filter(filename_prefix, filename_suffix);
		f.listFiles(matching_files, filter);

		int next_available_index = -1;

		for (File *p_matching_file : matching_files) {
			string file_name = p_matching_file->getName();
			regex expression("[.][^.]+$");
			string replacement("");
			string file_name_prefix = regex_replace(file_name, expression,
					replacement);
			string file_counter = file_name_prefix.substr(
					filename_prefix.length());
			int counter = atoi(file_counter.c_str());
			if (counter > next_available_index) {
				next_available_index = counter;
			}
			delete p_matching_file;
		}

		next_available_index++;

		string new_filename = filename_prefix + std::to_string(next_available_index);
		return writeToFile(directory + new_filename + "." + filename_suffix);
	}

	bool writeToFile(const string& file_path) {
		PrintWriter out(file_path);
		bool success = writeToDiskInternal(out);
		out.close();
		return success;
	}

	bool writeToDiskInternal(PrintWriter& out) {
		bool success = true;
		// Write header
		int oldest_index;
		int num_to_write;
		if (num_valid_samples > 0) {
			if (num_valid_samples == history_size) {
				oldest_index = curr_index + 1;
				if (oldest_index >= history_size) {
					oldest_index = 0;
				}
				num_to_write = num_valid_samples;
			} else { /* List is not completely filled */
				oldest_index = 0;
				num_to_write = curr_index + 1;
			}
			T *p_first_entry = history[oldest_index];
			vector<string> quantity_names;
			IQuantity& quantity = p_first_entry->getQuantity();
			bool is_quantity_container = quantity.getContainedQuantityNames(
					quantity_names);
			/* Write Header */
			string header = "Timestamp";
			if (is_quantity_container) {
				for (string& quantity_name : quantity_names) {
					header += "," + value_name + "." + quantity_name;
				}
			} else {
				header += "," + value_name;
			}
			out.println(header);

			for (int i = 0; i < num_to_write; i++) {
				vector<string> value_string;
				T* p_entry_to_write = history[oldest_index++];
				quantity = p_entry_to_write->getQuantity();
				value_string.push_back(std::to_string(p_entry_to_write->getTimestamp()));
				value_string.push_back(",");
				if (is_quantity_container) {
					vector<IQuantity *> contained_quantities;
					quantity.getContainedQuantities(contained_quantities);
					int index = 0;
					for (IQuantity* contained_quantity : contained_quantities) {
						vector<string> printable_string;
						if (index++ != 0) {
							value_string.push_back(",");
						}
						contained_quantity->getPrintableString(
								printable_string);
						for (auto string_part : printable_string) {
							value_string.push_back(string_part);
						}
					}
				} else {
					quantity.getPrintableString(value_string);
				}
				string output_string;
				for (auto string_part : value_string) {
					output_string += string_part;
				}
				out.println(output_string);
				if (oldest_index >= history_size) {
					oldest_index = 0;
				}
				if (oldest_index == curr_index) {
					break;
				}
			}
		}
		return success;
	}
};

#endif /* SRC_TIME_THREADSAFEINTERPOLATINGTIMEHISTORY_H_ */
