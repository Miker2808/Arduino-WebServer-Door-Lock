import requests

if __name__ == "__main__":
    url = "http://10.0.10.250" # the IP of the door look
    my_data = {"DATA":"","goto":"opendoor"}
    try:
        post_answer = requests.post(url, data=my_data)
    except Exception as e:
        print("expected")