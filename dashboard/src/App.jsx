import React, { useState, useEffect, useRef } from 'react';
import mqtt from 'mqtt'; // ต้องติดตั้ง: npm install mqtt --save

// ⚙️ MQTT CONFIGURATION
// ใช้ Public Broker ของ HiveMQ (สำหรับ Demo)
// Port 8000 คือ WebSockets (จำเป็นสำหรับ React/Browser)
const MQTT_BROKER = 'ws://broker.hivemq.com:8000/mqtt';

// 🔑 TOPICS (ตั้งชื่อให้ไม่ซ้ำกับคนอื่น)
// Topic สำหรับรับข้อมูลจาก ESP32
const TOPIC_DATA = 'cmu/iot/benz/server-room/data'; 
// Topic สำหรับส่งคำสั่งไป ESP32 (เช่น เปิดพัดลม)
const TOPIC_COMMAND = 'cmu/iot/benz/server-room/command'; 

export default function ServerRoomDashboard() {
    const clientRef = useRef(null);
    const [connectionStatus, setConnectionStatus] = useState('Disconnected');
    
    const [data, setData] = useState({
        temperature: 0,
        humidity: 0,
        gas: 0,
        status: 'normal',
        timestamp: new Date()
    });

    const [history, setHistory] = useState([]);
    const [fanManual, setFanManual] = useState(false);
    const [alertActive, setAlertActive] = useState(false);

    useEffect(() => {
        // 1. เชื่อมต่อ MQTT Broker
        console.log('Connecting to MQTT Broker...');
        const mqttClient = mqtt.connect(MQTT_BROKER);

        mqttClient.on('connect', () => {
            console.log('✅ Connected to MQTT!');
            setConnectionStatus('Connected');
            
            // 2. Subscribe รอฟังข้อมูล
            mqttClient.subscribe(TOPIC_DATA, (err) => {
                if (!err) {
                    console.log(`📡 Subscribed to ${TOPIC_DATA}`);
                }
            });
        });

        mqttClient.on('message', (topic, message) => {
            if (topic === TOPIC_DATA) {
                try {
                    // 3. แปลงข้อความ JSON ที่ได้รับมาเป็น Object
                    const payload = JSON.parse(message.toString());
                    console.log('📥 Received:', payload);

                    const newData = {
                        temperature: parseFloat(payload.temp),
                        humidity: parseFloat(payload.humi),
                        gas: parseInt(payload.gas),
                        status: payload.status, // 'normal', 'warning', 'critical'
                        timestamp: new Date()
                    };

                    setData(newData);
                    setAlertActive(payload.status !== 'normal');

                    // อัปเดตกราฟประวัติ
                    setHistory(prev => [...prev.slice(-29), {
                        time: newData.timestamp.toLocaleTimeString(),
                        temp: newData.temperature,
                        humi: newData.humidity,
                        gas: newData.gas / 41 // Scale for graph visualization
                    }]);

                } catch (error) {
                    console.error('Error parsing MQTT message:', error);
                }
            }
        });

        mqttClient.on('error', (err) => {
            console.error('MQTT Connection Error:', err);
            setConnectionStatus('Error');
        });

        clientRef.current = mqttClient;

        return () => {
            if (mqttClient) mqttClient.end();
        };
    }, []);

    // ฟังก์ชันส่งคำสั่งกลับไป ESP32
    const sendCommand = (command, value) => {
        if (clientRef.current) {
            clientRef.current.publish(TOPIC_COMMAND, JSON.stringify({ command, value }));
            console.log(`📤 Sent Command: ${command}`);
        }
    };

    // ... (ส่วนการแสดงผล UI/Colors เหมือนเดิม) ...
    const getStatusColor = () => {
        switch (data.status) {
            case 'critical': return 'bg-red-500';
            case 'warning': return 'bg-yellow-500';
            default: return 'bg-green-500';
        }
    };
    const getStatusEmoji = () => {
        switch (data.status) {
            case 'critical': return '🔴';
            case 'warning': return '⚠️';
            default: return '✅';
        }
    };
    const getStatusText = () => {
        switch (data.status) {
            case 'critical': return 'CRITICAL!';
            case 'warning': return 'WARNING';
            default: return 'NORMAL';
        }
    };
    const getTempColor = () => {
        if (data.temperature > 40) return 'text-red-600';
        if (data.temperature > 35) return 'text-yellow-600';
        return 'text-green-600';
    };
    const getHumiColor = () => {
        if (data.humidity < 30 || data.humidity > 70) return 'text-yellow-600';
        return 'text-green-600';
    };
    const getGasColor = () => {
        if (data.gas > 3000) return 'text-red-600';
        if (data.gas > 2000) return 'text-yellow-600';
        return 'text-green-600';
    };

    return (
        <div className="min-h-screen bg-gradient-to-br from-indigo-600 via-purple-600 to-pink-500 p-4">
            <div className="max-w-7xl mx-auto">

                {/* Header Section */}
                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-2xl p-6 mb-6">
                    <div className="flex items-center justify-between mb-4">
                        <h1 className="text-3xl font-bold text-gray-800">
                            🖥️ Smart Server Room Guardian
                        </h1>
                        <div className="text-right">
                            <div className="text-sm text-gray-500">{data.timestamp.toLocaleTimeString()}</div>
                            <div className={`text-xs font-bold ${connectionStatus === 'Connected' ? 'text-green-500' : 'text-red-500'}`}>
                                MQTT: {connectionStatus}
                            </div>
                        </div>
                    </div>

                    <div className={`${getStatusColor()} text-white text-center py-4 rounded-2xl text-2xl font-bold transition-colors duration-500`}>
                        {getStatusEmoji()} {getStatusText()}
                    </div>

                    {alertActive && (
                        <div className="mt-4 bg-red-100 border-l-4 border-red-500 p-4 rounded animate-pulse">
                            <p className="text-red-700 font-semibold">
                                ⚠️ Alert Active - System requires attention!
                            </p>
                        </div>
                    )}
                </div>

                {/* Sensors Grid */}
                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-6">
                    {/* Temperature */}
                    <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6">
                        <div className="flex items-center justify-between mb-4">
                            <div className="text-gray-500 text-sm font-semibold">🌡️ TEMPERATURE</div>
                            {data.temperature > 35 && (
                                <span className="text-red-500 text-xs font-bold animate-bounce">HIGH</span>
                            )}
                        </div>
                        <div className={`text-5xl font-bold ${getTempColor()} mb-2`}>
                            {data.temperature.toFixed(1)}<span className="text-2xl text-gray-400 ml-2">°C</span>
                        </div>
                        <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                            <div className={`h-full ${getTempColor().replace('text', 'bg')} transition-all duration-1000`} style={{ width: `${Math.min(100, (data.temperature / 50) * 100)}%` }} />
                        </div>
                    </div>

                    {/* Humidity */}
                    <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6">
                        <div className="flex items-center justify-between mb-4">
                            <div className="text-gray-500 text-sm font-semibold">💧 HUMIDITY</div>
                        </div>
                        <div className={`text-5xl font-bold ${getHumiColor()} mb-2`}>
                            {data.humidity.toFixed(1)}<span className="text-2xl text-gray-400 ml-2">%</span>
                        </div>
                        <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                            <div className={`h-full ${getHumiColor().replace('text', 'bg')} transition-all duration-1000`} style={{ width: `${data.humidity}%` }} />
                        </div>
                    </div>

                    {/* Gas */}
                    <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6">
                        <div className="flex items-center justify-between mb-4">
                            <div className="text-gray-500 text-sm font-semibold">💨 GAS LEVEL</div>
                        </div>
                        <div className={`text-5xl font-bold ${getGasColor()} mb-2`}>
                            {data.gas}<span className="text-2xl text-gray-400 ml-2">/4095</span>
                        </div>
                        <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                            <div className={`h-full ${getGasColor().replace('text', 'bg')} transition-all duration-1000`} style={{ width: `${(data.gas / 4095) * 100}%` }} />
                        </div>
                    </div>
                </div>

                {/* History Graph */}
                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6 mb-6">
                    <h3 className="text-xl font-bold text-gray-800 mb-4">📈 Real-time Data Stream</h3>
                    <div className="h-48 flex items-end justify-between gap-1 border-b border-l border-gray-200 p-2">
                        {history.map((item, i) => (
                            <div key={i} className="flex-1 flex flex-col items-center gap-1">
                                <div className="w-full bg-red-400 rounded-t opacity-80 hover:opacity-100 transition" style={{ height: `${(item.temp / 50) * 100}%` }} title={`Temp: ${item.temp}`} />
                                <div className="w-full bg-blue-400 rounded-t opacity-80 hover:opacity-100 transition" style={{ height: `${item.gas}%` }} title={`Gas: ${item.gas}`} />
                            </div>
                        ))}
                    </div>
                </div>

                {/* Controls */}
                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6">
                    <h2 className="text-2xl font-bold text-gray-800 mb-6">🎛️ Manual Controls (MQTT)</h2>
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <button
                            className="bg-red-500 hover:bg-red-600 text-white font-bold py-6 px-8 rounded-2xl shadow-lg transition active:scale-95"
                            onClick={() => {
                                setAlertActive(false);
                                sendCommand('RESET_ALARM', 1); // ส่ง MQTT
                            }}
                        >
                            🔕 Reset Alarm
                        </button>

                        <button
                            className={`${fanManual ? 'bg-blue-700' : 'bg-blue-500'} hover:bg-blue-600 text-white font-bold py-6 px-8 rounded-2xl shadow-lg transition active:scale-95`}
                            onClick={() => {
                                const newState = !fanManual;
                                setFanManual(newState);
                                sendCommand('FAN_CONTROL', newState ? 1 : 0); // ส่ง MQTT
                            }}
                        >
                            🌀 Emergency Fan: {fanManual ? 'ON' : 'OFF'}
                        </button>
                    </div>
                </div>

            </div>
        </div>
    );
}