import matplotlib.pyplot as plt

# 数据
flow_size = [0, 2000, 2100, 2500, 6000, 10000, 20000, 30000, 50000, 80000, 200000, 1000000, 2000000, 5000000, 10000000, 30000000]
probability = [0.0, 0.0, 0.02, 0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.53, 0.6, 0.7, 0.8, 0.9, 0.97, 1]

# 绘制图像
plt.figure(figsize=(8, 6))
plt.plot(flow_size, probability, marker='o', linestyle='-', color='b')

# 设置标题和标签
plt.title('Probability Distribution')
plt.xlabel('Flow Size')
plt.ylabel('Probability')

# 显示图像
plt.grid(True)
plt.savefig("./flow.png")