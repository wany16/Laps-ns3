def read_network_file_with_id(filename):
    network = {}
    connections_with_id = []
    
    with open(filename, 'r') as file:
        line_counter = 0
        for line in file:
            line_counter += 1
            parts = list(map(int, line.split()))
            source_node = parts[0]
            target_nodes=parts[1]
            pathNum = parts[2]
            pathIds  = parts[3:]

            if len(pathIds) !=pathNum:
                print(f"Warning: pathNum {pathNum}  len(pathId) {len(pathIds)} h")

            connection_info = {
                'id': line_counter,
                'source': source_node,
                'target':target_nodes,
                'pathNum': pathNum,
                'pathIds': pathIds
            }
            connections_with_id.append(connection_info)

           

    return  connections_with_id
def read_pit_file_with_id(filename):
   
    pit_table = []
    
    with open(filename, 'r') as file:
        line_counter = 0
        for line in file:
            line_counter += 1
            parts = list(map(int, line.split()))
            pathId = parts[0]
            priority=parts[1]
            idx=int((len(parts)-2)/2)
            print(idx)
            portlist = parts[2:idx+2]
            nodelist  = parts[idx+2:]

            if (len(portlist)+1) !=len(nodelist):
                print(f"Warning: portlen+1 {portlist} != nodelistlen {nodelist}")

            pit_info = {
                'id': line_counter,
                'pathId': pathId,
                'priority':priority,
                'portlist': portlist,
                'nodelist': nodelist
            }
            pit_table.append(pit_info)
    return  pit_table

def write_PIT_file_with_pittable(output_filename, pit_table):
    with open(output_filename, 'w') as file:
        for pit in pit_table:
            for installId in ["0","1","2","3","4"]:
            
                file.write(f"{installId} {pit['pathId']} {pit['priority']}")
                for portId in pit['portlist']:
                    file.write(f" {portId}")
                for nodeId in pit['nodelist']:
                    file.write(f" {nodeId}")
                file.write("\n")

    
def write_network_file_with_id(output_filename, connections_with_id):
    with open(output_filename, 'w') as file:
        for conn in connections_with_id:
            for installId in ["0","1","2","3","4"]:
                
                file.write(f"{installId} {conn['source']} {conn['target']} {conn['pathNum']}")
                for pathId in conn['pathIds']:
                    file.write(f" {pathId}")
                file.write("\n")

# 使用示例
if __name__ == "__main__":
    #input_filename = r"D:\work\ChinamobileSuxiaoyan\中移苏小妍\code\ns3\Laps-ns3\ns-3.33\inputFiles\C00002\PST_S5_H4_L10.txt"  # 输入文件名
    #output_filename = r"D:\work\ChinamobileSuxiaoyan\中移苏小妍\code\ns3\Laps-ns3\ns-3.33\inputFiles\C00002\PST_S5_H4_L10_1.txt"  # 输出文件名
    
    # 读取并解析网络文件
    #connections_with_id = read_network_file_with_id(input_filename)
    # 将带有ID的连接信息写入新的文本文件
    #write_network_file_with_id(output_filename, connections_with_id)
    input_filename = r"D:\work\ChinamobileSuxiaoyan\中移苏小妍\code\ns3\Laps-ns3\ns-3.33\inputFiles\C00002\PIT_S5_H4_L10.txt"  # 输入文件名
    output_filename = r"D:\work\ChinamobileSuxiaoyan\中移苏小妍\code\ns3\Laps-ns3\ns-3.33\inputFiles\C00002\PIT_S5_H4_L10_1.txt"  # 输出文件名
    pit_table= read_pit_file_with_id(input_filename)
    write_PIT_file_with_pittable(output_filename,pit_table)
   
    