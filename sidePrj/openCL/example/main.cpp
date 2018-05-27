#include <iostream>
#include <vector>
#include <string>
#include <time.h>

#define __CL_ENABLE_EXCEPTIONS
#include <OpenCL/cl.hpp>

// Compute c = a + b.
static const char source[] =

    "kernel void add(\n"
    "       ulong n,\n"
    "       global const float *x1,\n"
    "       global const float *x2,\n"
    "       global float *y1\n"
    "       )\n"
    "{\n"
    "    size_t i = get_global_id(0);\n"
    "    y1[i] = sin((((((x1[i]+ x2[i]) + i))))) + x1[i] + x2[i];\n"
    "}\n";

int main() {
    const size_t N = 1024*1024;

    try {
	// Get list of OpenCL platforms.
	std::vector<cl::Platform> platform;
	cl::Platform::get(&platform);

	if (platform.empty()) {
	    std::cerr << "OpenCL platforms not found." << std::endl;
	    return 1;
	}

	// Get first available GPU device which supports double precision.
	cl::Context context;
	std::vector<cl::Device> device;
	for(auto p = platform.begin(); device.empty() && p != platform.end(); p++) {
	    std::vector<cl::Device> pldev;

	    try {
		p->getDevices(CL_DEVICE_TYPE_GPU, &pldev);

		for(auto d = pldev.begin(); device.empty() && d != pldev.end(); d++) {
		    if (!d->getInfo<CL_DEVICE_AVAILABLE>()) continue;

		    std::string ext = d->getInfo<CL_DEVICE_EXTENSIONS>();

		    device.push_back(*d);
		    context = cl::Context(device);
		}
	    } catch(...) {
		device.clear();
	    }
	}

	if (device.empty()) {
	    std::cerr << "GPUs with double precision not found." << std::endl;
	    return 1;
	}

	std::cout << device[0].getInfo<CL_DEVICE_NAME>() << std::endl;

	// Create command queue.
	cl::CommandQueue queue(context, device[0]);

	// Compile OpenCL program for found device.
	cl::Program program(context, cl::Program::Sources(
		    1, std::make_pair(source, strlen(source))
		    ));

	try {
	    program.build(device);
	} catch (const cl::Error&) {
	    std::cerr
		<< "OpenCL compilation error" << std::endl
		<< program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device[0])
		<< std::endl;
	    return 1;
	}

	cl::Kernel add(program, "add");

	// Prepare input data.
	std::vector<float> a(N, 0);
	std::vector<float> b(N, 0.1);
	std::vector<float> c(N);

	// Allocate device buffers and transfer input data to device.
	cl::Buffer A(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		a.size() * sizeof(float), a.data());

	cl::Buffer B(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		b.size() * sizeof(float), b.data());

	cl::Buffer C(context, CL_MEM_READ_WRITE,
		c.size() * sizeof(float));

	// Set kernel parameters.
	add.setArg(0, static_cast<cl_ulong>(N));
	add.setArg(1, A);
	add.setArg(2, B);
	add.setArg(3, A);


  clock_t t = clock();
  printf("START.....\n");
	// Launch kernel on the compute device.
  for(int i=0;i<10000;i++)
  {
  	queue.enqueueNDRangeKernel(add, cl::NullRange, N, cl::NullRange);
  }
  queue.finish();

  printf("elapse:%fms \n", ((float)clock() - t) / CLOCKS_PER_SEC * 1000);

	// Get result back to host.
	queue.enqueueReadBuffer(A, CL_TRUE, 0, c.size() * sizeof(float), c.data());

	// Should get '3' here.
  for (int i = 0; i < 10; ++i)
  {
    fprintf(stderr, ">>>>%f\n", c[i]);

  }
    } catch (const cl::Error &err) {
	std::cerr
	    << "OpenCL error: "
	    << err.what() << "(" << err.err() << ")"
	    << std::endl;
	return 1;
    }
}
