run: all
	./cache_bench
	python3 analyze.py

shield-run: all
	@echo "Enabling cpuset controller in cgroup v2..."
	sudo sh -c "echo '+cpuset' > /sys/fs/cgroup/cgroup.subtree_control"
	@echo "Setting up CPU Shield on Core 1..."
	sudo mkdir -p /sys/fs/cgroup/benchmark
	sudo sh -c "echo 1 > /sys/fs/cgroup/benchmark/cpuset.cpus"
	sudo sh -c "echo 0 > /sys/fs/cgroup/benchmark/cpuset.mems"
	@echo "Running Isolated Benchmark..."
	# Run bench and capture results
	sudo sh -c "echo $$$$ > /sys/fs/cgroup/benchmark/cgroup.procs && ./cache_bench && python3 analyze.py"
	@echo "Cleaning up..."
	# Move processes back to root and delete
	-sudo rmdir /sys/fs/cgroup/benchmark

# Induce the anomaly
test-noise: all
	@echo "--- INDUCING CACHE CONTENTION ---"
	./cache_bench --noise
	python3 analyze.py

all:
	g++ -O3 -march=native main.cpp -o cache_bench -lpthread

clean:
	rm -f cache_bench *.csv
