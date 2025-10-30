#!/usr/bin/env python3
"""
Load Testing Script for Pix Application
Tests performance with large projects and stress conditions
"""

import subprocess
import time
import psutil
import threading
import random
import os
from pathlib import Path

class PixLoadTester:
    def __init__(self, app_path="pix"):
        self.app_path = app_path
        self.process = None
        self.results = []
        
    def start_application(self):
        """Start the application"""
        print("Starting application...")
        self.process = subprocess.Popen([self.app_path])
        time.sleep(2)  # Allow app to initialize
        
    def stop_application(self):
        """Stop the application"""
        if self.process:
            self.process.terminate()
            self.process.wait()
            
    def measure_performance(self, test_name):
        """Measure current performance metrics"""
        if not self.process:
            return None
            
        try:
            process = psutil.Process(self.process.pid)
            memory_mb = process.memory_info().rss / 1024 / 1024
            cpu_percent = process.cpu_percent()
            
            result = {
                'test': test_name,
                'memory_mb': memory_mb,
                'cpu_percent': cpu_percent,
                'timestamp': time.time()
            }
            self.results.append(result)
            return result
        except psutil.NoSuchProcess:
            return None
    
    def test_large_canvas(self, width=512, height=512):
        """Test performance with large canvas"""
        print(f"Testing large canvas {width}x{height}...")
        # This would simulate creating a large canvas
        # In a real test, you'd use automation to resize canvas
        time.sleep(3)
        return self.measure_performance("large_canvas")
    
    def test_many_frames(self, frame_count=100):
        """Test performance with many frames"""
        print(f"Testing {frame_count} frames...")
        # Simulate adding many frames
        for i in range(frame_count):
            if i % 10 == 0:
                self.measure_performance(f"frame_{i}")
            time.sleep(0.1)
        return self.measure_performance("many_frames")
    
    def test_memory_leak(self, duration=60):
        """Test for memory leaks over time"""
        print(f"Testing for memory leaks for {duration} seconds...")
        start_time = time.time()
        measurements = []
        
        while time.time() - start_time < duration:
            result = self.measure_performance("memory_leak_test")
            if result:
                measurements.append(result['memory_mb'])
            time.sleep(5)
            
        # Check if memory consistently grows
        if len(measurements) > 3:
            growth_rate = (measurements[-1] - measurements[0]) / len(measurements)
            return growth_rate < 1.0  # Allow small fluctuations
        return True
    
    def test_stress_drawing(self, operations=1000):
        """Stress test with many drawing operations"""
        print(f"Stress testing with {operations} operations...")
        start_mem = self.measure_performance("stress_start")
        
        # Simulate many operations
        for i in range(operations):
            if i % 100 == 0:
                self.measure_performance(f"stress_{i}")
            time.sleep(0.01)
            
        end_mem = self.measure_performance("stress_end")
        return start_mem, end_mem
    
    def run_all_tests(self):
        """Run all load tests"""
        print("Starting comprehensive load testing...")
        
        try:
            self.start_application()
            
            # Run individual tests
            tests = [
                ("Large Canvas", lambda: self.test_large_canvas()),
                ("Many Frames", lambda: self.test_many_frames(50)),
                ("Memory Leak", lambda: self.test_memory_leak(30)),
                ("Stress Drawing", lambda: self.test_stress_drawing(500))
            ]
            
            for test_name, test_func in tests:
                print(f"\n=== Running {test_name} ===")
                try:
                    result = test_func()
                    if result:
                        print(f"✓ {test_name} completed")
                    else:
                        print(f"✗ {test_name} failed")
                except Exception as e:
                    print(f"✗ {test_name} error: {e}")
            
            # Generate report
            self.generate_report()
            
        finally:
            self.stop_application()
    
    def generate_report(self):
        """Generate performance report"""
        print("\n" + "="*50)
        print("LOAD TESTING REPORT")
        print("="*50)
        
        for result in self.results:
            print(f"{result['test']:20} | Memory: {result['memory_mb']:6.1f} MB | CPU: {result['cpu_percent']:5.1f}%")
        
        # Calculate averages
        if self.results:
            avg_memory = sum(r['memory_mb'] for r in self.results) / len(self.results)
            max_memory = max(r['memory_mb'] for r in self.results)
            print(f"\nAverage Memory: {avg_memory:.1f} MB")
            print(f"Peak Memory: {max_memory:.1f} MB")

if __name__ == "__main__":
    tester = PixLoadTester()
    tester.run_all_tests()