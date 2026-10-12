import { get } from 'lodash-es';
import { METHOD2FUNC } from '@/constant/ws';
import type { TMethodParams } from '@/types/websocket';

export default async function handleMessage({ data }: { data: string }) {
    try {
        const decodedData = JSON.parse(data);
        if (!decodedData?.method) return;

        const method = get(decodedData, 'method') as keyof TMethodParams;
        if (method && METHOD2FUNC[method]) {
            const handler = METHOD2FUNC[method];
            // 确保传入的参数类型符合预期
            await handler(decodedData);
        } else {
            console.warn(`No handler found for method: ${method}`);
        }
    } catch (error) {
        console.log(`handle message ${data} with error: ${error}`);
    }
}
