import json
import os

try:
    request = json.dumps({"id": 1, "method": "Target.getBrowserContexts"}) + "\n" + "\0"
    # request = (
    #     json.dumps(
    #         {
    #             "id": 1,
    #             "method": "Target.createTarget",
    #             "params": {"url": "about:blank"},
    #         }
    #     )
    #     + "\n"
    #     + "\0"
    # )

    # Write message to named pipe
    with open("cdp_pipein", "w") as cdp_pipein:
        cdp_pipein.write(request)

    # Read response from named pipe
    with open("cdp_pipeout", "r") as cdp_pipeout:
        response = cdp_pipeout.readline()
    print("Response: " + response)

except Exception as e:
    print("[exception][message] : ", str(e))
