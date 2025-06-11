
import React from 'react';
import { Link, useLocation } from 'react-router-dom';
import { createPageUrl } from '@/utils';
import { Activity, Settings, TerminalSquare, BookOpen, Cpu } from 'lucide-react'; // Added BookOpen, Cpu

export default function Layout({ children, currentPageName }) {
  const navItems = [
    { name: 'Oscilloscope', href: createPageUrl('Oscilloscope'), icon: Activity },
    { name: 'Documentation', href: createPageUrl('Documentation'), icon: BookOpen },
    { name: 'ESP32Firmware', href: createPageUrl('ESP32Firmware'), icon: Cpu }, // Added ESP32 Firmware page
    // { name: 'Settings', href: createPageUrl('Settings'), icon: Settings }, // Future page
  ];

  return (
    <div className="flex flex-col h-screen bg-gray-900 text-gray-100">
      <header className="bg-gray-800 shadow-md">
        <div className="container mx-auto px-4 py-3 flex items-center justify-between">
          <div className="flex items-center space-x-2">
            <TerminalSquare className="h-8 w-8 text-cyan-400" />
            <h1 className="text-xl font-semibold tracking-tight text-white">
              Miniscope
            </h1>
          </div>
          <nav className="flex space-x-2">
            {navItems.map((item) => (
              <Link
                key={item.name}
                to={item.href}
                className={`px-3 py-2 rounded-md text-sm font-medium flex items-center space-x-2 transition-colors
                  ${currentPageName === item.name
                    ? 'bg-cyan-500 text-white'
                    : 'text-gray-300 hover:bg-gray-700 hover:text-white'
                  }`}
              >
                <item.icon className="h-4 w-4" />
                <span>{item.name}</span>
              </Link>
            ))}
          </nav>
        </div>
      </header>
      <main className="flex-1 overflow-auto p-4">
        {children}
      </main>
      <footer className="bg-gray-800 text-center p-3 text-xs text-gray-400 border-t border-gray-700">
        ESP32 Miniscope Interface
      </footer>
    </div>
  );
}
