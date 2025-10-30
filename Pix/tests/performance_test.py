#!/usr/bin/env python3
"""
Performance Benchmarking Script
Measures specific operations for performance regression testing
"""

import time
import statistics
import subprocess

class PerformanceBenchmark:
    def __init__(self):
        self.results = {}
        
    def measure_operation(self, operation_name, operation_func, iterations=10):
        """Measure execution time of an operation"""
        times = []
        
        for i in range(iterations):
            start_time = time.time()
            operation_func()
            end_time = time.time()
            times.append((end_time - start_time) * 1000)  # Convert to ms
            
        avg_time = statistics.mean(times)
        std_dev = statistics.stdev(times) if len(times) > 1 else 0
        
        self.results[operation_name] = {
            'average_ms': avg_time,
            'std_dev': std_dev,
            'min_ms': min(times),
            'max_ms': max(times)
        }
        
        print(f"{operation_name}: {avg_time:.2f}ms ± {std_dev:.2f}ms")
        
    def benchmark_canvas_operations(self):
        """Benchmark various canvas operations"""
        print("Benchmarking Canvas Operations...")
        
        # These would be actual function calls to your application
        # For now, using placeholders
        
        def create_canvas():
            # Simulate canvas creation
            time.sleep(0.01)
            
        def resize_canvas():
            # Simulate canvas resize
            time.sleep(0.02)
            
        def add_frame():
            # Simulate adding frame
            time.sleep(0.005)
            
        def draw_operation():
            # Simulate drawing
            time.sleep(0.001)
            
        self.measure_operation("Create Canvas", create_canvas)
        self.measure_operation("Resize Canvas", resize_canvas)
        self.measure_operation("Add Frame", add_frame)
        self.measure_operation("Draw Operation", draw_operation, iterations=50)
        
    def benchmark_file_operations(self):
        """Benchmark file I/O operations"""
        print("\nBenchmarking File Operations...")
        
        def save_project():
            # Simulate project save
            time.sleep(0.05)
            
        def load_project():
            # Simulate project load
            time.sleep(0.05)
            
        def export_gif():
            # Simulate GIF export
            time.sleep(0.1)
            
        self.measure_operation("Save Project", save_project)
        self.measure_operation("Load Project", load_project)
        self.measure_operation("Export GIF", export_gif)
        
    def generate_report(self):
        """Generate performance report"""
        print("\n" + "="*50)
        print("PERFORMANCE BENCHMARK REPORT")
        print("="*50)
        
        for op_name, result in self.results.items():
            print(f"{op_name:20} | Avg: {result['average_ms']:6.2f}ms | "
                  f"Min: {result['min_ms']:6.2f}ms | Max: {result['max_ms']:6.2f}ms")
        
        # Save results to file for comparison
        with open("performance_results.txt", "w") as f:
            for op_name, result in self.results.items():
                f.write(f"{op_name},{result['average_ms']},{result['std_dev']}\n")

if __name__ == "__main__":
    benchmark = PerformanceBenchmark()
    benchmark.benchmark_canvas_operations()
    benchmark.benchmark_file_operations()
    benchmark.generate_report()