import json
import os
import sys
import webbrowser
import pandas as pd
import plotly.express as px

def visualize_heap(json_file_path="build/heap_state.json",
                   output_html="build/heap_visual.html"):
    try:
        with open(json_file_path, 'r') as f:
            heap_data = json.load(f)
    except FileNotFoundError:
        print(f"Error: Could not find {json_file_path}. Make sure the C++ allocator ran first.")
        return

    all_blocks = []
    for order_group in heap_data.get("orders", []):
        curr_order = order_group["order"]
        for block in order_group.get("blocks", []):
            block["order"] = curr_order
            all_blocks.append(block)
    
    if not all_blocks:
        print("No blocks found in the JSON file.")
        return


    
    df = pd.DataFrame(all_blocks)
    df['start_int'] = df['start'].apply(lambda x: int(x, 16))
    df = df.sort_values('start_int')

    df['heap_row'] = 'Heap State'
    
    fig = px.bar(
        df,
        x="size",
        y="heap_row",
        color="status",
        orientation='h',
        color_discrete_map={"Free": "#2ecc71", "Allocated": "#e74c3c"},
        hover_data=["start", "order", "requested", "internalFragmentation"],
	labels={
		"start": "Starting Addr",
		"internalFragmentation": "Internal Frag (Bytes)",
		"requested": "Requested Size (Bytes)"
	},
        title=f"Buddy Allocator Live Memory Map (Total Free: {heap_data['metrics']['freeBlocks']} blocks)"
	)
        
    fig.update_yaxes(title="", showticklabels=False) 
        
    fig.write_html(output_html)
    print(f"Success! Interactive visual saved to '{output_html}'.", file=sys.stderr)

    abs_path = "file://" + os.path.realpath(output_html)
    webbrowser.open(abs_path)

if __name__ == "__main__":
    visualize_heap()
