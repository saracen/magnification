#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <list>
#include <mutex>
#include <regex>
#include <condition_variable>

struct settings
{
	int levels = 5;
	double alpha = 20.0;
	double lambda_c = 20.0;
	double cutoff_frequency_high = 0.4;
	double cutoff_frequency_low = 0.05;
	double chrom_attenuation = 0.1;
	double exaggeration_factor = 2.0;

	std::string filename;
};

std::ostream &operator<<(std::ostream &strm, const settings &s)
{
	return strm
		   << "levels: " << s.levels << std::endl
		   << "alpha: " << s.alpha << std::endl
		   << "lambda_c: " << s.lambda_c << std::endl
		   << "cutoff_frequency_high: " << s.cutoff_frequency_high << std::endl
		   << "cutoff_frequency_low: " << s.cutoff_frequency_low << std::endl
		   << "chrom_attenuation: " << s.chrom_attenuation << std::endl
		   << "exaggeration_factor: " << s.exaggeration_factor << std::endl
		   << "filename: " << s.filename << std::endl;
}

int main(int argc, char **argv)
{
	settings settings;
	static const std::tuple<std::regex, std::function<void(std::smatch)>> options[] = {
		{std::regex("^levels=(\\d+)$"), [&](std::smatch m) { settings.levels = std::stoi(m.str(1), nullptr); }},
		{std::regex("^alpha=([+-]?((\\d+(\\.\\d*)?)|(\\.\\d+)))$"), [&](std::smatch m) { settings.alpha = std::stod(m.str(1), nullptr); }},
		{std::regex("^cutoff_frequency_low=([+-]?((\\d+(\\.\\d*)?)|(\\.\\d+)))$"), [&](std::smatch m) { settings.cutoff_frequency_low = std::stod(m.str(1), nullptr); }},
		{std::regex("^cutoff_frequency_high=([+-]?((\\d+(\\.\\d*)?)|(\\.\\d+)))$"), [&](std::smatch m) { settings.cutoff_frequency_high = std::stod(m.str(1), nullptr); }},
		{std::regex("^lambda_c=([+-]?((\\d+(\\.\\d*)?)|(\\.\\d+)))$"), [&](std::smatch m) { settings.lambda_c = std::stod(m.str(1), nullptr); }},
		{std::regex("^chrom_attenuation=([+-]?((\\d+(\\.\\d*)?)|(\\.\\d+)))$"), [&](std::smatch m) { settings.chrom_attenuation = std::stod(m.str(1), nullptr); }},
		{std::regex("^exaggeration_factor=([+-]?((\\d+(\\.\\d*)?)|(\\.\\d+)))$"), [&](std::smatch m) { settings.exaggeration_factor = std::stod(m.str(1), nullptr); }},
		{std::regex("^(.*)$"), [&](std::smatch m) { settings.filename = m.str(1); }},
	};

	// Parse options/settings
	for (int i = 1; i < argc; i++)
	{
		for (auto &option : options)
		{
			std::string str = argv[i];
			std::smatch m;
			std::regex_match(str, m, std::get<0>(option));
			if (m.size())
			{
				std::get<1>(option)(m);
			}
		}
	}

	// Print out settings
	std::cout << settings;

	auto frame_num = 0;
	std::vector<cv::Mat> low_pass1;
	std::vector<cv::Mat> low_pass2;
	std::vector<cv::Mat> filtered;

	cv::VideoCapture capture(settings.filename);
	auto w = capture.get(cv::CAP_PROP_FRAME_WIDTH);
	auto h = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	bool input_complete = false;
	std::mutex input_mutex;
	std::condition_variable input_notifier;
	std::list<std::vector<cv::Mat>> input_queue;

	bool output_complete = false;
	std::mutex output_mutex;
	std::condition_variable output_notifier;
	std::list<std::tuple<cv::Mat, cv::Mat>> output_queue;

	// Input thread:
	// Capture input, convert color space and perform pyramid decomposition
	auto input_thread = std::thread([&]() {
		cv::Mat frame;
		while (true)
		{
			if (!capture.read(frame))
			{
				input_complete = true;
				input_notifier.notify_all();
				return;
			}

			auto input = frame.clone();

			// Convert to LAB color space
			input.convertTo(input, CV_32FC3, 1.0 / 255.0f);
			cv::cvtColor(input, input, cv::COLOR_BGR2Lab);

			// Pyramid decomposition
			std::vector<cv::Mat> pyramid;
			{
				auto current = input;
				for (int l = 0; l < settings.levels; l++)
				{
					cv::Mat down, up;

					pyrDown(current, down);
					pyrUp(down, up, current.size());

					pyramid.push_back(current - up);
					current = down;
				}

				pyramid.push_back(current);
				pyramid.push_back(input);
			}

			// Add to input queue
			std::unique_lock<std::mutex> lock(input_mutex);
			input_queue.push_back(pyramid);
			input_notifier.notify_one();
		}
	});

	// Output thread:
	// Convert color space and display image
	auto output_thread = std::thread([&]() {
		std::tuple<cv::Mat, cv::Mat> output;
		while (true)
		{
			{
				std::unique_lock<std::mutex> lock(output_mutex);
				output_notifier.wait(lock, [&]() {
					return output_complete || !output_queue.empty();
				});
				if (output_complete && output_queue.empty())
				{
					return;
				}

				output = output_queue.front();
				output_queue.pop_front();
			}

			auto normal = std::get<0>(output);
			auto amplified = std::get<1>(output);

			// Convert back to RGB color space
			cv::cvtColor(normal, normal, cv::COLOR_Lab2BGR);
			normal.convertTo(normal, CV_8UC3, 255.0, 1.0 / 255.0);

			cv::cvtColor(amplified, amplified, cv::COLOR_Lab2BGR);
			amplified.convertTo(amplified, CV_8UC3, 255.0, 1.0 / 255.0);

			// Display
			cv::imshow("Input", normal);
			cv::imshow("Output", amplified);
			cv::waitKey(30);
		}
	});

	// Main thread:
	// Take frames from input thread, process, and push to output thread
	while (true)
	{
		auto t_start = std::chrono::high_resolution_clock::now();

		std::vector<cv::Mat> pyramid;
		{
			std::unique_lock<std::mutex> lock(input_mutex);
			input_notifier.wait(lock, [&]() {
				return input_complete || !input_queue.empty();
			});
			if (input_complete && input_queue.empty())
			{
				std::unique_lock<std::mutex> lock(output_mutex);
				output_complete = true;
				output_notifier.notify_all();

				break;
			}

			pyramid = input_queue.front();
			input_queue.pop_front();
		}

		cv::Mat input = pyramid[settings.levels + 1];

		if (frame_num == 0)
		{
			for (int l = 0; l < pyramid.size(); l++)
			{
				filtered.push_back(pyramid[l].clone());
				low_pass1.push_back(pyramid[l].clone());
				low_pass2.push_back(pyramid[l].clone());
			}
		}

		if (frame_num > 0)
		{
			auto delta = settings.lambda_c / 8.0 / (1.0 + settings.alpha);
			auto lambda = sqrt((float)(w * w + h * h)) / 3;

			// Process levels of pyramid
			std::vector<std::thread> workers;
			for (int level = settings.levels; level >= 0; level--)
			{
				workers.push_back(std::thread([&, level, lambda]() {
					// First or last level we mostly ignore
					if (level == settings.levels || level == 0)
					{
						filtered[level] *= 0;
						return;
					}

					// Temporal IIR Filter
					low_pass1[level] = (1 - settings.cutoff_frequency_high) * low_pass1[level] + settings.cutoff_frequency_high * pyramid[level];
					low_pass2[level] = (1 - settings.cutoff_frequency_low) * low_pass2[level] + settings.cutoff_frequency_low * pyramid[level];
					filtered[level] = low_pass1[level] - low_pass2[level];

					// Amplify
					auto current_alpha = (lambda / delta / 8 - 1) * settings.exaggeration_factor;
					filtered[level] *= std::min(settings.alpha, current_alpha);
				}));

				lambda /= 2.0;
			}

			std::for_each(workers.begin(), workers.end(), [](std::thread &t) {
				t.join();
			});
		}

		// Pyramid reconstruction
		cv::Mat motion;
		{
			auto current = filtered[settings.levels];
			for (int level = settings.levels - 1; level >= 0; --level)
			{
				cv::Mat up;
				pyrUp(current, up, filtered[level].size());
				current = up + filtered[level];
			}
			motion = current;
		}

		if (frame_num > 0)
		{
			// Attenuate I, Q channels
			cv::Mat planes[3];
			split(motion, planes);
			planes[1] = planes[1] * settings.chrom_attenuation;
			planes[2] = planes[2] * settings.chrom_attenuation;
			cv::merge(planes, 3, motion);

			// Combine
			motion = input + motion;
		}

		auto t_now = std::chrono::high_resolution_clock::now();
		std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start);

		std::cout << "frame: " << frame_num << ", took " << elapsed.count() << "ms" << std::endl;
		frame_num++;

		// Add to output queue
		std::unique_lock<std::mutex> lock(output_mutex);
		output_queue.push_back(std::tuple<cv::Mat, cv::Mat>(input, motion));
		output_notifier.notify_one();
	}

	input_thread.join();
	output_thread.join();

	return 0;
}
