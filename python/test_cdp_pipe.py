import json

try:
    # pipein = open("cdp_pipein", "w")
    pipeout = open("cdp_pipeout", "r")
     
    # Example DevTools Protocol message
    request = json.dumps({"id": 1, "method": "Target.getBrowserContexts"}) + "\n" + "\0"

    # Write message to named pipe
    # pipein.write(request)

    # Read response from named pipe
    response = pipeout.readline()
    print("Response: " + response)
except Exception as e:
    print("[exception][message] : %s", str(e))
