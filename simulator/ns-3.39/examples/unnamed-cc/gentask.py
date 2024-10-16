#!/usr/bin/env python3

import numpy as np
import pandas as pd
import random

def generate_ml_traffic(task_id, num_tasks):
    data = []
    for task in range(1, num_tasks + 1):
        # Number of iterations between 300 and 400
        iterations = random.randint(300, 400)
        
        # Poisson-distributed total training time (communication + computation)
        total_time = np.random.poisson(400)
        
        # Random communication/computation ratio
        comm_ratio = random.uniform(0.1, 0.9)
        comm_time = total_time * comm_ratio
        comp_time = total_time * (1 - comm_ratio)
        
        # Append one line per task
        data.append([task_id + task, iterations, comm_time, comp_time])

    # Convert to DataFrame and save/display it
    df = pd.DataFrame(data, columns=["Task", "Number of Iterations", "Communication Time (s)", "Computation Time (s)"])
    return df

task_id_start = 1000  # Starting ID for tasks
num_tasks = 5  # Number of tasks to generate
df_output = generate_ml_traffic(task_id_start, num_tasks)
df_output_path = "ml_traffic.csv"
df_output.to_csv(df_output_path, index=False)