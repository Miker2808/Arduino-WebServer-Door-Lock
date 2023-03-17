import requests

webserver_ip = "192.168.0.1"

if __name__ == "__main__":
    url = f"http://{webserver_ip}" # the IP of the door look
    my_data = {"DATA":"","goto":"opendoor"}
    try:
        post_answer = requests.post(url, data=my_data)
    except Exception as e:
        print("Received Exception: ")
        print(e)