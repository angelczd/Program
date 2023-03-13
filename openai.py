import openai
from openai.api_client import DefaultApi

openai.api_key = "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"
api_instance = DefaultApi()

# 定义请求函数
def ask_gpt(prompt):
    # 发送请求
    response = api_instance.completions.create(
      engine="davinci",
      prompt=prompt,
      max_tokens=2048,
      n=1,
      stop=None,
      temperature=0.7
    )
    # 获取回复
    message = response.choices[0].text
    # 去掉回车符
    message = message.strip()
    return message

# 控制台输入问题并输出回复
while True:
    prompt = input("请输入您的问题：")
    if prompt.lower() == 'exit':
        break
    message = ask_gpt(prompt)
    print(message)
